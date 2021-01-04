#include <stdint.h>
#include <stdbool.h>

#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/DerivedTypes.h"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "hardbound"

#define SETBOUND_ASM  \
  "li x17, 98\n"      \
  "mv x10, $0\n"      \
  "mv x11, $1\n"      \
  "mv x12, $2\n"      \
  "ecall"
#define SETBOUND_CONS \
  "r,r,r,~{x17},~{x10},~{x11},~{x12}"

namespace {
  struct Hardbound : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    LLVMContext context;

    Hardbound() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "Hardbound: ";
      errs().write_escaped(F.getName()) << '\n';

      bool modified = false;
      for (auto it = F.begin(); it != F.end(); it++) {
        BasicBlock &bb = *it;

        for (auto instrIt = bb.begin(); instrIt != bb.end(); instrIt++) {
          Instruction &instr = *instrIt;
          IRBuilder<> builder = IRBuilder<>(&instr);

          StoreInst *storeInst = dyn_cast<StoreInst>(instrIt);
          if (storeInst) {
            if (runOnStoreInstr(builder, *storeInst))
              modified = true;
          }
        }
      }

      return modified;
    }

  private:

    Instruction *buildSetbound(IRBuilder<> &builder, Value *pointer, Value *base, size_t numbytes) {
      auto u32 = IntegerType::get(context, 32);

      InlineAsm *Asm = InlineAsm::get(
          FunctionType::get(builder.getVoidTy(), {u32, u32, u32}, false),
          StringRef(SETBOUND_ASM),
          StringRef(SETBOUND_CONS),
          true);

      auto size = ConstantInt::get(u32, numbytes);
      auto ptrInt = builder.CreatePtrToInt(pointer, u32);
      auto baseInt = builder.CreatePtrToInt(base, u32);

      return builder.CreateCall(Asm, {ptrInt, baseInt, size}, "");
    }

    Instruction *runOnStoreInstr(IRBuilder<> &builder, StoreInst &storeInst) {
      Value *value   = storeInst.getValueOperand();
      Value *pointer = storeInst.getPointerOperand();

      if (!value->getType()->isPointerTy())
        return nullptr;

      errs() << '\t' << storeInst << '\n';
      errs() << "\t\t" << "VALUE: " << *value << '\n';
      errs() << "\t\t" << "POINTER: " << *pointer << "\n\n";

      ssize_t numbytes = getValueByteSize(value);
      if (numbytes == -1)
        return nullptr;

      Instruction *setboundInstr = buildSetbound(builder, pointer, value, numbytes);
      errs() << "\n\n" << "GENERATED: " << *setboundInstr << '\n';

      return setboundInstr;
    }

    ssize_t getValueByteSize(Value *value) {
      const AllocaInst *allocaInst = dyn_cast<AllocaInst>(value);
      const GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(value);
      const ConstantExpr *consExpr = dyn_cast<ConstantExpr>(value);

      ssize_t numbytes = -1;
      if (allocaInst) {
        auto allocated = allocaInst->getAllocatedType();
        numbytes = allocated->getScalarSizeInBits() / CHAR_BIT;
      } else if (elemPtrInst) {
        auto sourceElem = elemPtrInst->getSourceElementType();
        if (!sourceElem->isArrayTy())
          return -1;

        auto elems = sourceElem->getArrayNumElements();
        auto elem_size = sourceElem->getArrayElementType()->getScalarSizeInBits();

        numbytes = elems * (elem_size / CHAR_BIT);
      } else if (consExpr) {
        if (consExpr->getOpcode() != Instruction::GetElementPtr)
          return -1;

        Value *operand   = consExpr->getOperand(0);
        PointerType *ptr = dyn_cast<PointerType>(operand->getType());
        if (!ptr)
          return -1;

        Type *t = ptr->getElementType();
        if (!t->isArrayTy())
          return -1;

        auto elems = t->getArrayNumElements();
        auto elem_size = t->getArrayElementType()->getScalarSizeInBits();

        numbytes = elems * (elem_size / CHAR_BIT);
      }

      return numbytes;
    }
  };
}

char Hardbound::ID = 0;
static RegisterPass<Hardbound> X("hardbound", "hardbound setbounds compiler pass");

/* vim: set et ts=2 sw=2: */
