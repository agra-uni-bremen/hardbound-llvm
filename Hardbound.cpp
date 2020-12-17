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

#define SETBOUND_ASM \
  "li a7, 98\n"      \
  "mv a0, $0\n"      \
  "mv a1, $1\n"      \
  "mv a2, $2\n"      \
  "ecall"
#define SETBOUND_CONS \
  "r,r,r,~{x17},~{x10},~{x11},~{x12}"

namespace {
  struct Hardbound : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid

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
      LLVMContext context;
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

      const AllocaInst *allocaInst = dyn_cast<AllocaInst>(value);
      if (!allocaInst)
        return nullptr;

      auto allocated = allocaInst->getAllocatedType();
      size_t numbytes = allocated->getScalarSizeInBits() / 8;

      Instruction *setboundInstr = buildSetbound(builder, pointer, value, numbytes);
      errs() << "\n\n" << "GENERATED: " << *setboundInstr << '\n';

      return setboundInstr;
    }
  };
}

char Hardbound::ID = 0;
static RegisterPass<Hardbound> X("hardbound", "hardbound bounds compiler pass");

/* vim: set et ts=2 sw=2: */
