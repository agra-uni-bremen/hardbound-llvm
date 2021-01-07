#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

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

  /**
   * Array2Pointer compiler pass.
   *
   * This compiler pass rewrites direct array accesses to access through
   * pointer arithmetic. This is neccessary because the former cannot be
   * instrumented by the Hardbound pass and therefore not checked for
   * spatial memory safety violations. The rewrite peformed by the
   * Array2Pointer pass should not affect observable program behaviour.
   */
  struct Array2Pointer : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    DataLayout *DL;

    Array2Pointer() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "Array2Pointer: ";
      errs().write_escaped(F.getName()) << '\n';

      DataLayout dataLayout = F.getParent()->getDataLayout();
      DL = &dataLayout;

      bool modified = false;
      for (auto it = F.begin(); it != F.end(); it++) {
        BasicBlock &bb = *it;

        for (auto instrIt = bb.begin(); instrIt != bb.end(); instrIt++) {
          LoadInst *loadInst = dyn_cast<LoadInst>(instrIt);
          if (loadInst) {
            IRBuilder<> builder = IRBuilder<>(loadInst);
            auto newLoad = runOnLoadInstr(builder, loadInst);
            if (!newLoad)
              continue;

            ReplaceInstWithInst(bb.getInstList(), instrIt, newLoad);
            errs() << bb << '\n';
            modified = true;
          }
        }
      }

      return modified;
    }

  private:

    void shouldBeInBounds(Value *value) {
      GetElementPtrInst *elemInstr = dyn_cast<GetElementPtrInst>(value);
      if (!elemInstr)
        return;

      elemInstr->setIsInBounds(true);
    }

    Value *getElemPtrIndex(GetElementPtrInst *instr) {
      /* From the LLVM Language Reference Manual:
       *   […] the second index indexes a value of the type pointed to.
       */
      auto indices = instr->getNumIndices();
      if (indices < 2)
        llvm_unreachable("unexpected number of GEP indicies");
      auto it = std::next(instr->idx_begin(), 1);

      return *it;
    }

    LoadInst *transformArrayAccess(IRBuilder<> &builder, GetElementPtrInst *gep, ArrayType *array) {
      auto elemType = array->getElementType();
      auto ptrType = PointerType::get(elemType, 0);

      // Alloc space for pointer to array on the stack.
      auto allocInstr = builder.CreateAlloca(ptrType);
      allocInstr->setAlignment(DL->getPointerPrefAlignment());

      // Create a pointer to the first element of the array.
      Value *elemPtr = builder.CreateGEP(gep->getPointerOperand(), builder.getInt32(0));
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
      Value *index = getElemPtrIndex(gep);
      Value *elem = builder.CreateGEP(elemType, loadInst, index);
      shouldBeInBounds(elem);

      // Load the value returned by the getelementptr instruction.
      auto finalLoad = builder.CreateLoad(elemType, elem);
      return finalLoad;
    }

    Instruction *runOnLoadInstr(IRBuilder<> &builder, LoadInst *loadInst) {
      Value *pointer = loadInst->getPointerOperand();
      GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(pointer);
      if (!elemPtrInst)
        return nullptr;

      Type *opType = elemPtrInst->getPointerOperandType();
      PointerType *ptr = dyn_cast<PointerType>(opType);
      if (!ptr)
        return nullptr;

      ArrayType *array = dyn_cast<ArrayType>(ptr->getElementType());
      if (!array)
        return nullptr;

      auto newLoad = transformArrayAccess(builder, elemPtrInst, array);
      newLoad->setAlignment(loadInst->getAlign());

      errs() << "oldLoad: " << *loadInst << '\n';
      errs() << "newLoad: " << *newLoad << '\n';

      return newLoad;
    }
  };

  /**
   * Setbound compiler pass.
   *
   * This compiler pass instruments store instructions, inserting a
   * setbound instruction for each store which represents a pointer.
   * This allows bounds-checking to be peformed in the Hardware as
   * described in the original Hardbound paper.
   */
  struct Setbound : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid

    LLVMContext context;
    DataLayout *DL;

    Setbound() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "Setbound: ";
      errs().write_escaped(F.getName()) << '\n';

      DataLayout dataLayout = F.getParent()->getDataLayout();
      DL = &dataLayout;

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
      auto i32 = builder.getInt32Ty();

      InlineAsm *Asm = InlineAsm::get(
          FunctionType::get(builder.getVoidTy(), {i32, i32, i32}, false),
          StringRef(SETBOUND_ASM),
          StringRef(SETBOUND_CONS),
          true);

      auto ptrInt = builder.CreatePtrToInt(pointer, i32);
      auto baseInt = builder.CreatePtrToInt(base, i32);

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

    Value *xsizeof(IRBuilder<> &builder, Type *type) {
      if (type->isArrayTy())
        return getArraySize(builder, type);

      StructType *tstruct = dyn_cast<StructType>(type);
      if (tstruct) {
        const StructLayout *sl = DL->getStructLayout(tstruct);
        return builder.getInt32(sl->getSizeInBytes());
      }

      auto size = type->getScalarSizeInBits() / CHAR_BIT;
      return builder.getInt32(size);
    }

    Value *getArraySize(IRBuilder<> &builder, Type *type) {
      if (!type->isArrayTy())
        return nullptr;

      auto elems = type->getArrayNumElements();
      auto elem_size = xsizeof(builder, type->getArrayElementType());

      auto elem_size_const = dyn_cast<llvm::ConstantInt>(elem_size);
      if (!elem_size)
        llvm_unreachable("elem_size is not constant");

      size_t total_size = elems * elem_size_const->getZExtValue();
      return builder.getInt32(total_size);
    }

    Value *baseOffset(IRBuilder<> &builder, Value *offset, Type *source) {
        if (!source->isArrayTy())
          llvm_unreachable("getelementptr on non array type");

        auto elemType = source->getArrayElementType();
        auto elemSize = xsizeof(builder, elemType);

        return builder.CreateMul(offset, elemSize);
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
        return baseOffset(builder, offset, source);
    }

    Value *getValueByteSize(IRBuilder<> &builder, Value *value) {
      /* Discard pointer casts as they are(?) irrelevant for this analysis. */
      value = value->stripPointerCasts();

      const AllocaInst *allocaInst = dyn_cast<AllocaInst>(value);
      const GetElementPtrInst *elemPtrInst = dyn_cast<GetElementPtrInst>(value);
      const ConstantExpr *consExpr = dyn_cast<ConstantExpr>(value);
      const GlobalVariable *globalVar = dyn_cast<GlobalVariable>(value);

      Value *numbytes = nullptr;
      if (allocaInst) { /* pointer to stack-based scalar */
        auto allocated = allocaInst->getAllocatedType();
        numbytes = xsizeof(builder, allocated);
      } else if (elemPtrInst) { /* pointer to stack-based buffer */
        auto sourceElem = elemPtrInst->getSourceElementType();

        numbytes = xsizeof(builder, sourceElem);
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

        /* the second index indexes a value of the type pointed to */
        Value *index = consExpr->getOperand(2);
        Type *source = ptr->getElementType();

        Value *offset = baseOffset(builder, index, source);
        numbytes = builder.CreateSub(xsizeof(builder, source), offset);
      } else if (globalVar) { /* pointer to global scalar */
        PointerType *ptr = dyn_cast<PointerType>(globalVar->getType());
        if (!ptr)
          return nullptr;

        numbytes = xsizeof(builder, ptr->getElementType());
      }

      return numbytes;
    }
  };

}

char Array2Pointer::ID = 0;
static RegisterPass<Array2Pointer> A("array2pointer", "hardbound array2pointer compiler pass");

char Setbound::ID = 0;
static RegisterPass<Setbound> S("setbound", "hardbound setbounds compiler pass");

/* vim: set et ts=2 sw=2: */
