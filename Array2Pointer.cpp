#include "Array2Pointer.h"
#include "Utility.h"

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

  // Create a pointer to the given element of the array.
  Value *elemPtr = allocBuilder.CreateGEP(array, index);
  shouldBeInBounds(elemPtr);

  // Store pointer to array in stack space created by alloca.
  auto ptr = allocBuilder.CreatePointerCast(elemPtr, ptrType);
  auto storeInst = allocBuilder.CreateStore(ptr, allocInstr);
  storeInst->setAlignment(DL->getPointerPrefAlignment());

  // Next step: Load ptr and return it, allows performing
  // a new access at a new index using getelementptr on
  // this returned loadInst.
  auto loadInst = builder->CreateLoad(ptrType, allocInstr);
  loadInst->setAlignment(DL->getPointerPrefAlignment());

  return loadInst;
}

Value *
Array2Pointer::convertGEP(Value *newPtr, ArrayType *array, User *oldInst)
{
  auto destTySize = xsizeof(builder, DL, array);

  // Unroll the getelementptr instruction. In LLVM IR the getelementptr
  // instruction can contain multiple indices. The first index must be
  // interpreted in terms of the pointer value passed as the second
  // argument. This interpretation may change if we change as we change
  // the type of the pointer argument.
  //
  // Iterate over all indices (these start at GEP operand 1), rewrite
  // the first indices and use seperate GEP instructions for all
  // following indices.
  //
  // XXX: Is it sufficient to simply rewrite the first index?
  Value *prevArray = newPtr;
  for (size_t i = 1; i < oldInst->getNumOperands(); i++) {
    Value *index = oldInst->getOperand(i);

    // The first index always indexes the pointer value given as the
    // second argument, based on the size of this pointer value.
    if (i == 1)
      index = builder->CreateMul(destTySize, index);

    prevArray = builder->CreateGEP(prevArray, index);
  }

  assert(prevArray != newPtr); /* at least one iteration required */
  return prevArray;
}

Value *
Array2Pointer::convertGEP(GetElementPtrInst *gep)
{
  Value *pointer = gep->getPointerOperand();
  Type *opType = gep->getPointerOperandType();

  PointerType *ptr = dyn_cast<PointerType>(opType);
  if (!ptr)
    return nullptr;

  ArrayType *array = dyn_cast<ArrayType>(ptr->getElementType());
  if (!array)
    return nullptr;

  auto newPtr = getArrayPointer(pointer, array, builder->getInt32(0));
  return convertGEP(newPtr, array, gep);
}

Value *
Array2Pointer::convertGEP(ConstantExpr *consExpr)
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

  auto newPtr = getArrayPointer(arrayPtr, arrayTy, builder->getInt32(0));
  return convertGEP(newPtr, arrayTy, consExpr);
}

Value *
Array2Pointer::value2array(Value *v)
{
  if (ConstantExpr *consExpr = dyn_cast<ConstantExpr>(v)) {
    return convertGEP(consExpr);
  } else if (GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(v)) {
    return convertGEP(elemPtrInst);
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

    Value *ptr = convertGEP(consExpr);
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
