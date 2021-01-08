#ifndef HARDBOUND_ARRAY2POINTER
#define HARDBOUND_ARRAY2POINTER

#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

/**
 * Array2Pointer compiler pass.
 *
 * This compiler pass rewrites direct array accesses to access through
 * pointer arithmetic. This is neccessary because the former cannot be
 * instrumented by the Hardbound pass and therefore not checked for
 * spatial memory safety violations. The rewrite peformed by the
 * Array2Pointer pass should not affect observable program behaviour.
 */
struct Array2Pointer : public llvm::FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  llvm::DataLayout *DL;

  Array2Pointer() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;

  private:

  void shouldBeInBounds(llvm::Value *value);
  llvm::Value *getElemPtrIndex(llvm::GetElementPtrInst *instr);

  llvm::Value *getArrayPointer(llvm::IRBuilder<> &builder, llvm::Value *array, llvm::ArrayType *arrayTy, llvm::Value *index);
  llvm::LoadInst *transformArrayAccess(llvm::IRBuilder<> &builder, llvm::GetElementPtrInst *gep, llvm::ArrayType *arrayTy);

  llvm::Instruction *runOnLoadInstr(llvm::IRBuilder<> &builder, llvm::LoadInst *loadInst);
  llvm::Instruction *runOnCallInst(llvm::IRBuilder<> &builder, llvm::CallInst *callInst);
};

#endif

/* vim: set et ts=2 sw=2: */
