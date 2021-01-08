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

    for (auto instrIt = bb.begin(); instrIt != bb.end(); instrIt++) {
      Instruction *newInstr = nullptr;
      if (LoadInst *loadInst = dyn_cast<LoadInst>(instrIt)) {
        IRBuilder<> builder = IRBuilder<>(loadInst);
        newInstr = runOnLoadInstr(builder, loadInst);
      } else if (StoreInst *storeInst = dyn_cast<StoreInst>(instrIt)) {
        IRBuilder<> builder = IRBuilder<>(storeInst);
        newInstr = runOnStoreInstr(builder, storeInst);
      } else if (CallInst *callInst = dyn_cast<CallInst>(instrIt)) {
        IRBuilder<> builder = IRBuilder<>(callInst);
        newInstr = runOnCallInst(builder, callInst);
      }

      if (newInstr) {
        ReplaceInstWithInst(bb.getInstList(), instrIt, newInstr);
        modified = true;
      }
    }

    errs() << bb << '\n';
  }

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
Array2Pointer::getElemPtrIndex(GetElementPtrInst *instr)
{
  /* From the LLVM Language Reference Manual:
   *   […] the second index indexes a value of the type pointed to.
   */
  auto indices = instr->getNumIndices();
  if (indices < 2)
    llvm_unreachable("unexpected number of GEP indicies");
  auto it = std::next(instr->idx_begin(), 1);

  return *it;
}

Value *
Array2Pointer::getArrayPointer(IRBuilder<> &builder, Value *array, ArrayType *arrayTy, Value *index)
{
  auto elemType = arrayTy->getElementType();
  auto ptrType = PointerType::get(elemType, 0);

  // Alloc space for pointer to array on the stack.
  auto allocInstr = builder.CreateAlloca(ptrType);
  allocInstr->setAlignment(DL->getPointerPrefAlignment());

  // Create a pointer to the first element of the array.
  Value *elemPtr = builder.CreateGEP(array, builder.getInt32(0));
  shouldBeInBounds(elemPtr);

  // Store pointer to array in stack space created by alloca.
  auto ptr = builder.CreatePointerCast(elemPtr, ptrType);
  auto storeInst = builder.CreateStore(ptr, allocInstr);
  storeInst->setAlignment(DL->getPointerPrefAlignment());

  // At this point: Pointer to array at index 0 is stored on stack
  // This store should be detected and instrumented by the Setbound pass.
  //
  // Next step: Load ptr and access the previously accessed array
  // index using the stored pointer later instrumented with Setbound.
  auto loadInst = builder.CreateLoad(ptrType, allocInstr);
  loadInst->setAlignment(DL->getPointerPrefAlignment());

  // Using the loaded pointer, create a getelementptr instruction
  // which access the value previously accessed directly.
  Value *elem = builder.CreateGEP(elemType, loadInst, index);
  shouldBeInBounds(elem);

  return elem;
}

Value *
Array2Pointer::getArrayPointer(IRBuilder<> &builder, GetElementPtrInst *gep, ArrayType *arrayTy)
{
  Value *index = getElemPtrIndex(gep);
  return getArrayPointer(builder, gep->getPointerOperand(), arrayTy, index);
}

Value *
Array2Pointer::getArrayPointer(IRBuilder<> &builder, ConstantExpr *consExpr)
{
  if (consExpr->getOpcode() != Instruction::GetElementPtr)
    return nullptr;

  Value *arrayPtr  = consExpr->getOperand(0);
  PointerType *ptr = dyn_cast<PointerType>(arrayPtr->getType());
  if (!ptr)
    return nullptr;

  ArrayType *arrayTy = dyn_cast<ArrayType>(ptr->getElementType());
  if (!arrayTy)
    return nullptr;

  Value *index = consExpr->getOperand(2);
  return getArrayPointer(builder, arrayPtr, arrayTy, index);
}

Value *
Array2Pointer::value2arrayPtr(IRBuilder<> &builder, Value *v)
{
  if (ConstantExpr *consExpr = dyn_cast<ConstantExpr>(v)) {
    return getArrayPointer(builder, consExpr);
  } else if (GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(v)) {
    Type *opType = elemPtrInst->getPointerOperandType();
    PointerType *ptr = dyn_cast<PointerType>(opType);
    if (!ptr)
      return nullptr;

    ArrayType *array = dyn_cast<ArrayType>(ptr->getElementType());
    if (!array)
      return nullptr;

    return getArrayPointer(builder, elemPtrInst, array);
  }

  return nullptr;
}

Instruction *
Array2Pointer::runOnStoreInstr(IRBuilder<> &builder, StoreInst *storeInst)
{
  Value *value = storeInst->getValueOperand();
  Value *arrayPointer = value2arrayPtr(builder, value);
  if (!arrayPointer)
    return nullptr;

  auto newStore = builder.CreateStore(arrayPointer,
      storeInst->getPointerOperand(),
      storeInst->isVolatile());
  newStore->setAlignment(storeInst->getAlign());

  return newStore;
}

Instruction *
Array2Pointer::runOnLoadInstr(IRBuilder<> &builder, LoadInst *loadInst)
{
  Value *pointer = loadInst->getPointerOperand();
  Value *arrayPointer = value2arrayPtr(builder, pointer);
  if (!arrayPointer)
    return nullptr;

  auto newLoad = builder.CreateLoad(loadInst->getType(), arrayPointer);
  newLoad->setAlignment(loadInst->getAlign());

  return newLoad;
}

Instruction *
Array2Pointer::runOnCallInst(IRBuilder<> &builder, CallInst *callInst)
{
  bool modified = true;

  for (size_t i = 0; i < callInst->arg_size(); i++) {
    Value *arg = callInst->getArgOperand(i);

    ConstantExpr *consExpr = dyn_cast<ConstantExpr>(arg);
    if (!consExpr)
      return nullptr;

    Value *ptr = getArrayPointer(builder, consExpr);
    if (!ptr)
      return nullptr;

    callInst->setArgOperand(i, ptr);
    modified = true;
  }

  return (modified) ? callInst : nullptr;
}

/* vim: set et ts=2 sw=2: */
