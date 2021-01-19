#include <assert.h>
#include <stdint.h>

#include "Setbound.h"
#include "Utility.h"

#define SETBOUND_ASM  \
  "li x17, 98\n"      \
  "mv x10, $0\n"      \
  "mv x11, $1\n"      \
  "mv x12, $2\n"      \
  "ecall"
#define SETBOUND_CONS \
  "r,r,r,~{x17},~{x10},~{x11},~{x12}"

using namespace llvm;

bool
Setbound::runOnFunction(Function &F)
{
  errs() << "Setbound: ";
  errs().write_escaped(F.getName()) << '\n';

  DataLayout dataLayout = F.getParent()->getDataLayout();
  DL = &dataLayout;

  bool modified = false;
  for (auto it = F.begin(); it != F.end(); it++) {
    BasicBlock &bb = *it;

    for (auto instrIt = bb.begin(); instrIt != bb.end(); instrIt++) {
      Instruction *instr = cast<Instruction>(instrIt);

      // Make sure builder places generated setbound instruction
      // after the store instruction used to create the pointer.
      Instruction *next = instr->getNextNode();
      auto instrBuilder = IRBuilder<>((next) ? next : instr);
      builder = &instrBuilder;

      if (StoreInst *storeInst = dyn_cast<StoreInst>(instr)) {
        if (runOnStoreInstr(*storeInst))
          modified = true;
      }
    }
  }

  return modified;
}

Instruction *
Setbound::buildSetbound(Value *pointer, Value *base, Value *numbytes)
{
  if (isInstrumented(base)) {
    errs() << "Already instrumented: " << *base << '\n';
    return nullptr;
  }

  auto i32 = builder->getInt32Ty();

  InlineAsm *Asm = InlineAsm::get(
      FunctionType::get(builder->getVoidTy(), {i32, i32, i32}, false),
      StringRef(SETBOUND_ASM),
      StringRef(SETBOUND_CONS),
      true);

  auto ptrInt = builder->CreatePtrToInt(pointer, i32);
  auto baseInt = builder->CreatePtrToInt(base, i32);

  return builder->CreateCall(Asm, {ptrInt, baseInt, numbytes}, "");
}

Instruction *
Setbound::runOnStoreInstr(StoreInst &storeInst)
{
  Value *value   = storeInst.getValueOperand();
  Value *pointer = storeInst.getPointerOperand();

  if (!value->getType()->isPointerTy())
    return nullptr;

  errs() << '\t' << storeInst << '\n';
  errs() << "\t\t" << "VALUE: " << *value << '\n';
  errs() << "\t\t" << "POINTER: " << *pointer << "\n\n";

  Value *numbytes = getValueByteSize(value);
  if (!numbytes)
    return nullptr;

  Instruction *setboundInstr = buildSetbound(pointer, value, numbytes);
  if (setboundInstr)
    errs() << "\n\n" << "GENERATED: " << *setboundInstr << '\n';

  return setboundInstr;
}

bool
Setbound::isInstrumented(Value *value)
{
  // If we create a pointer from an existing pointer we don't need
  // to add a setbound call for this new pointer since the bounds
  // are already propagated from the existing one. Consider:
  //
  //   int *ptr1 = …;
  //   int offset = 5;
  //   int *ptr2 = ptr1 + offset;
  //
  // In this case no setbound call is needed for ptr2.

  auto stripped = value->stripPointerCastsAndAliases();
  if (const auto gep = dyn_cast<GetElementPtrInst>(stripped)) {
    Value *operand = gep->getPointerOperand();
    auto stripped = operand->stripPointerCastsAndAliases();

    return stripped->getType()->isPointerTy();
  }

  return false;
}

Value *
Setbound::getValueByteSize(Value *value)
{
  /* Discard pointer casts as they are(?) irrelevant for this analysis. */
  value = value->stripPointerCasts();

  Value *numbytes = nullptr;
  if (const auto allocaInst = dyn_cast<AllocaInst>(value)) { /* pointer to stack-based scalar */
    auto allocated = allocaInst->getAllocatedType();
    numbytes = xsizeof(builder, DL, allocated);
  } else if (const auto elemPtrInst = dyn_cast<GetElementPtrInst>(value)) { /* pointer to stack-based buffer */
    auto sourceElem = elemPtrInst->getSourceElementType();

    numbytes = xsizeof(builder, DL, sourceElem);
  } else if (const auto consExpr = dyn_cast<ConstantExpr>(value)) { /* pointer to constant/global buffer */
    if (consExpr->getOpcode() != Instruction::GetElementPtr)
      return nullptr;

    Value *operand = consExpr->getOperand(0);
    PointerType *ptr = dyn_cast<PointerType>(operand->getType());
    if (!ptr)
      return nullptr;

    numbytes = xsizeof(builder, DL, ptr->getElementType());
  } else if (const auto globalVar = dyn_cast<GlobalVariable>(value)) { /* pointer to global scalar */
    PointerType *ptr = dyn_cast<PointerType>(globalVar->getType());
    if (!ptr)
      return nullptr;

    numbytes = xsizeof(builder, DL, ptr->getElementType());
  }

  return numbytes;
}

/* vim: set et ts=2 sw=2: */
