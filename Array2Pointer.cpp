#include "Array2Pointer.h"

using namespace llvm;

bool
Array2Pointer::runOnFunction(Function &F)
{
  errs() << "Array2Pointer: ";
  errs().write_escaped(F.getName()) << '\n';

  DataLayout dataLayout = F.getParent()->getDataLayout();
  DL = &dataLayout;

  bool modified = false;
  for (auto it = F.begin(); it != F.end(); it++) {
    BasicBlock &bb = *it;
    currentBlock = &bb;

    for (auto instrIt = bb.begin(); instrIt != bb.end(); instrIt++) {
      Instruction *newInstr = nullptr;
      Instruction *instr = cast<Instruction>(instrIt);

      auto instrBuilder = IRBuilder<>(instr);
      builder = &instrBuilder;

      /* Check all operands for getelementptr constant expressions. */
      newInstr = checkInstrOperands(instr);
      if (newInstr) {
        modified = true;
        continue;
      }

      if (LoadInst *loadInst = dyn_cast<LoadInst>(instr)) {
        newInstr = runOnLoadInstr(loadInst);
      } else if (StoreInst *storeInst = dyn_cast<StoreInst>(instr)) {
        newInstr = runOnStoreInstr(storeInst);
      }

      if (newInstr) {
        ReplaceInstWithInst(bb.getInstList(), instrIt, newInstr);
        modified = true;
      }
    }

    /* if (modified) */
    /*   errs() << bb << '\n'; */
  }

  errs() << F << '\n';
  return modified;
}

void
Array2Pointer::shouldBeInBounds(Value *value)
{
  GetElementPtrInst *elemInstr = dyn_cast<GetElementPtrInst>(value);
  if (!elemInstr)
    return;

  elemInstr->setIsInBounds(true);
}

Value *
Array2Pointer::getArrayPointer(Value *array, ArrayType *arrayTy, Value *index)
{
  // We use this builder to setup our pointer to the given array.
  // This code is likely located at the beginning of a function
  // or basic block and will be instrumented by hardbound.
  Instruction *inst = dyn_cast<Instruction>(array);
  if (!inst) {
    if (dyn_cast<GlobalVariable>(array))
      inst = currentBlock->getFirstNonPHI();
    else
      llvm_unreachable("expected instruction or global variable");
  }
  Instruction *next = inst->getNextNode();
  IRBuilder<> allocBuilder((next) ? next : inst);

  auto elemType = arrayTy->getElementType();
  auto ptrType = PointerType::get(elemType, 0);

  // Alloc space for pointer to array on the stack.
  auto allocInstr = allocBuilder.CreateAlloca(ptrType);
  allocInstr->setAlignment(DL->getPointerPrefAlignment());

  // Create a pointer to the first element of the array.
  Value *elemPtr = allocBuilder.CreateGEP(array, allocBuilder.getInt32(0));
  shouldBeInBounds(elemPtr);

  // Store pointer to array in stack space created by alloca.
  auto ptr = allocBuilder.CreatePointerCast(elemPtr, ptrType);
  auto storeInst = allocBuilder.CreateStore(ptr, allocInstr);
  storeInst->setAlignment(DL->getPointerPrefAlignment());

  // At this point: Pointer to array at index 0 is stored on stack
  // This store should be detected and instrumented by the Setbound pass.
  //
  // The remaning code will rewrite the array access. Using
  // this->builder instead of allocBuilder.

  // Next step: Load ptr and access the previously accessed array
  // index using the stored pointer later instrumented with Setbound.
  auto loadInst = builder->CreateLoad(ptrType, allocInstr);
  loadInst->setAlignment(DL->getPointerPrefAlignment());

  // Using the loaded pointer, create a getelementptr instruction
  // which access the value previously accessed directly.
  Value *elem = builder->CreateGEP(elemType, loadInst, index);
  shouldBeInBounds(elem);

  return elem;
}

Value *
Array2Pointer::getArrayPointer(GetElementPtrInst *gep)
{
  Type *opType = gep->getPointerOperandType();
  PointerType *ptr = dyn_cast<PointerType>(opType);
  if (!ptr)
    return nullptr;

  ArrayType *array = dyn_cast<ArrayType>(ptr->getElementType());
  if (!array)
    return nullptr;

  /* From the LLVM Language Reference Manual:
   *   […] the second index indexes a value of the type pointed to.
   */
  Value *index = gep->getOperand(2);
  return getArrayPointer(gep->getPointerOperand(), array, index);
}

Value *
Array2Pointer::getArrayPointer(ConstantExpr *consExpr)
{
  if (consExpr->getOpcode() != Instruction::GetElementPtr)
    return nullptr;

  Value *arrayPtr = consExpr->getOperand(0);
  if (ConstantExpr *e = dyn_cast<ConstantExpr>(arrayPtr))
    arrayPtr = e->getOperand(0); // XXX: nested expression, what if nested again?

  PointerType *ptr = dyn_cast<PointerType>(arrayPtr->getType());
  if (!ptr)
    return nullptr;

  ArrayType *arrayTy = dyn_cast<ArrayType>(ptr->getElementType());
  if (!arrayTy)
    return nullptr;

  auto numOps = consExpr->getNumOperands();
  assert(numOps >= 2);

  Value *index = consExpr->getOperand(numOps - 1);
  return getArrayPointer(arrayPtr, arrayTy, index);
}

Value *
Array2Pointer::value2array(Value *v)
{
  if (ConstantExpr *consExpr = dyn_cast<ConstantExpr>(v)) {
    return getArrayPointer(consExpr);
  } else if (GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(v)) {
    return getArrayPointer(elemPtrInst);
  } else {
    return nullptr;
  }
}

Instruction *
Array2Pointer::checkInstrOperands(Instruction *inst)
{
  bool modified = false;
  for (size_t i = 0; i < inst->getNumOperands(); i++) {
    Value *operand = inst->getOperand(i);
    ConstantExpr *consExpr = dyn_cast<ConstantExpr>(operand);
    if (!consExpr)
      return nullptr;

    Value *ptr = getArrayPointer(consExpr);
    if (!ptr)
      return nullptr;

    inst->setOperand(i, ptr);
    modified = true;
  }

  return (modified) ? inst : nullptr;
}

Instruction *
Array2Pointer::runOnStoreInstr(StoreInst *storeInst)
{
  Value *value = storeInst->getValueOperand();
  Value *pointer = storeInst->getPointerOperand();

  // Distinguish the following cases:
  //   1. Store where value is a pointer (e.g. where the address of the
  //      buffer is stored somewhere, like `int *buf = &buf[5]`).
  //   2. Store where the pointer operand (address where value should be
  //      stored) points to a getelementptr, e.g. `buf[5] = '\0'`.

  StoreInst *newStore = nullptr;
  if (Value *arrayPointer = value2array(value)) {
    newStore = builder->CreateStore(arrayPointer,
        pointer, storeInst->isVolatile());
  } else if (Value *arrayPointer = value2array(pointer)) {
    newStore = builder->CreateStore(value,
        arrayPointer, storeInst->isVolatile());
  }

  if (newStore)
    newStore->setAlignment(storeInst->getAlign());
  return newStore;
}

Instruction *
Array2Pointer::runOnLoadInstr(LoadInst *loadInst)
{
  Value *pointer = loadInst->getPointerOperand();
  Value *arrayPointer = value2array(pointer);
  if (!arrayPointer)
    return nullptr;

  auto newLoad = builder->CreateLoad(loadInst->getType(), arrayPointer);
  newLoad->setAlignment(loadInst->getAlign());

  return newLoad;
}

/* vim: set et ts=2 sw=2: */
