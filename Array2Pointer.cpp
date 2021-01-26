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
      Instruction *instr = cast<Instruction>(instrIt);
      if (updateConsExprs(instr)) {
        modified = true;
      } else if (auto *gep = dyn_cast<GetElementPtrInst>(instr)) {
        runOnGEP(gep);
        modified = true;
      }
    }
  }

  errs() << F << '\n';
  return modified;
}

Value *
Array2Pointer::convertGEP(Type *sElemType, Value *pointer) {
  // We use this builder to setup our pointer to the given array.
  // This code is likely located at the beginning of a function
  // or basic block and will be instrumented by hardbound.
  Instruction *inst = dyn_cast<Instruction>(pointer);
  if (!inst) {
    if (dyn_cast<GlobalVariable>(pointer))
      inst = currentBlock->getFirstNonPHI();
    else
      llvm_unreachable("expected instruction or global variable");
  }
  Instruction *next = inst->getNextNode();
  IRBuilder<> allocBuilder((next) ? next : inst);

  ArrayRef<Value*> indices;
  assert(indices.empty());

  // Create base GEP instruction without any indices.
  Value *baseGEP = allocBuilder.CreateGEP(sElemType, pointer, indices);

  // Create space for pointer on stack.
  auto ptrAlloc = allocBuilder.CreateAlloca(baseGEP->getType());
  ptrAlloc->setAlignment(DL->getPointerPrefAlignment());

  // Store pointer to baseGEP on stack.
  // This store will be instrumented by the setbound pass.
  auto storeInst = allocBuilder.CreateStore(baseGEP, ptrAlloc);
  storeInst->setAlignment(DL->getPointerPrefAlignment());

  // Load pointer to baseGEP from stack and return it.
  auto loadInst = allocBuilder.CreateLoad(baseGEP->getType(), ptrAlloc);
  loadInst->setAlignment(DL->getPointerPrefAlignment());

  return loadInst;
}

void
Array2Pointer::runOnGEP(GetElementPtrInst *gep)
{
  Type *sourceElem = gep->getSourceElementType();
  Value *pointer = gep->getPointerOperand();

  errs() << "sourceElem: " << *sourceElem << '\n';
  errs() << "pointer: " << *pointer << '\n';

  auto baseLoad = convertGEP(sourceElem, pointer);
  gep->setOperand(0, baseLoad);
}

bool
Array2Pointer::updateConsExprs(Instruction *inst)
{
  bool modified = false;
  for (size_t i = 0; i < inst->getNumOperands(); i++) {
    Value *operand = inst->getOperand(i);
    ConstantExpr *consExpr = dyn_cast<ConstantExpr>(operand);
    if (!consExpr)
      continue;

    if (consExpr->getOpcode() != Instruction::GetElementPtr)
      continue;
    const auto *go = cast<GEPOperator>(consExpr);

    Value *pointer = consExpr->getOperand(0);
    if (dyn_cast<ConstantExpr>(pointer))
      continue; // TODO: Handle nested expressions

    Type *sourceElem = go->getSourceElementType();
    auto baseLoad = convertGEP(sourceElem, pointer);

    // Convert ConstantExpr to non-constant GetElementPtrInst.
    // Taken from ConstantExpr::getAsInstruction().
    SmallVector<Value *, 4> ValueOperands(consExpr->op_begin(), consExpr->op_end());
    ArrayRef<Value*> Ops(ValueOperands);
    IRBuilder<> builder(inst);
    auto newGEP = builder.CreateGEP(sourceElem, baseLoad, Ops.slice(1));

    inst->setOperand(i, newGEP);
    modified = true;
  }

  return modified;
}

/* vim: set et ts=2 sw=2: */
