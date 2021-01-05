#include <assert.h>
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
    IntegerType *u32;
    DataLayout *DL;

    Hardbound() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "Hardbound: ";
      errs().write_escaped(F.getName()) << '\n';

      DataLayout dataLayout = F.getParent()->getDataLayout();
      DL = &dataLayout;

      u32 = IntegerType::get(context, 32);

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

    Instruction *buildSetbound(IRBuilder<> &builder, Value *pointer, Value *base, Value *numbytes) {
      InlineAsm *Asm = InlineAsm::get(
          FunctionType::get(builder.getVoidTy(), {u32, u32, u32}, false),
          StringRef(SETBOUND_ASM),
          StringRef(SETBOUND_CONS),
          true);

      auto ptrInt = builder.CreatePtrToInt(pointer, u32);
      auto baseInt = builder.CreatePtrToInt(base, u32);

      return builder.CreateCall(Asm, {ptrInt, baseInt, numbytes}, "");
    }

    Instruction *runOnStoreInstr(IRBuilder<> &builder, StoreInst &storeInst) {
      Value *value   = storeInst.getValueOperand();
      Value *pointer = storeInst.getPointerOperand();

      if (!value->getType()->isPointerTy())
        return nullptr;

      errs() << '\t' << storeInst << '\n';
      errs() << "\t\t" << "VALUE: " << *value << '\n';
      errs() << "\t\t" << "POINTER: " << *pointer << "\n\n";

      Value *numbytes = getValueByteSize(builder, value);
      if (!numbytes)
        return nullptr;

      Instruction *setboundInstr = buildSetbound(builder, pointer, value, numbytes);
      errs() << "\n\n" << "GENERATED: " << *setboundInstr << '\n';

      return setboundInstr;
    }

    Value *xsizeof(Type *type) {
      if (type->isArrayTy())
        return getArraySize(type);

      StructType *tstruct = dyn_cast<StructType>(type);
      if (tstruct) {
        const StructLayout *sl = DL->getStructLayout(tstruct);
        return ConstantInt::get(u32, sl->getSizeInBytes());
      }

      auto size = type->getScalarSizeInBits() / CHAR_BIT;
      return ConstantInt::get(u32, size);
    }

    Value *getArraySize(Type *type) {
      if (!type->isArrayTy())
        return nullptr;

      auto elems = type->getArrayNumElements();
      auto elem_size = xsizeof(type->getArrayElementType());

      auto elem_size_const = dyn_cast<llvm::ConstantInt>(elem_size);
      if (!elem_size)
        llvm_unreachable("elem_size is not constant");

      size_t total_size = elems * elem_size_const->getZExtValue();
      return ConstantInt::get(u32, total_size);
    }

    Value *baseOffset(IRBuilder<> &builder, const GetElementPtrInst *instr) {
        /* From the LLVM Language Reference Manual:
         *   […] the second index indexes a value of the type pointed to.
         */
        auto indices = instr->getNumIndices();
        if (indices < 2)
          return nullptr;

        auto it = std::next(instr->idx_begin(), 1);
        Value *offset = *it;

        auto source = instr->getSourceElementType();
        if (!source->isArrayTy())
          llvm_unreachable("getelementptr on non array type");

        auto elemType = source->getArrayElementType();
        auto elemSize = xsizeof(elemType);

        return builder.CreateMul(offset, elemSize);
    }

    Value *getValueByteSize(IRBuilder<> &builder, Value *value) {
      const AllocaInst *allocaInst = dyn_cast<AllocaInst>(value);
      const GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(value);
      const ConstantExpr *consExpr = dyn_cast<ConstantExpr>(value);
      const GlobalVariable *globalVar = dyn_cast<GlobalVariable>(value);

      Value *numbytes = nullptr;
      if (allocaInst) { /* pointer to stack-based scalar */
        auto allocated = allocaInst->getAllocatedType();
        numbytes = xsizeof(allocated);
      } else if (elemPtrInst) { /* pointer to stack-based buffer */
        auto sourceElem = elemPtrInst->getSourceElementType();

        numbytes = xsizeof(sourceElem);
        Value *offset = baseOffset(builder, elemPtrInst);

        assert(offset && numbytes);
        numbytes = builder.CreateSub(numbytes, offset);
      } else if (consExpr) { /* pointer to constant/global buffer */
        if (consExpr->getOpcode() != Instruction::GetElementPtr)
          return nullptr;

        Value *operand = consExpr->getOperand(0);
        PointerType *ptr = dyn_cast<PointerType>(operand->getType());
        if (!ptr)
          return nullptr;

        numbytes = getArraySize(ptr->getElementType());
      } else if (globalVar) { /* pointer to global scalar */
        PointerType *ptr = dyn_cast<PointerType>(globalVar->getType());
        if (!ptr)
          return nullptr;

        numbytes = xsizeof(ptr->getElementType());
      }

      return numbytes;
    }
  };
}

char Hardbound::ID = 0;
static RegisterPass<Hardbound> X("hardbound", "hardbound setbounds compiler pass");

/* vim: set et ts=2 sw=2: */
