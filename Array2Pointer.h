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

  llvm::BasicBlock *currentBlock;
  llvm::DataLayout *DL;

  Array2Pointer() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override;

private:

  /**
    * Returns a LoadInst which loads the result of a generated
    * GetElementPtrInst for the given source type and the given pointer.
    * Storing the GetElementPtr result on the stack allows instrumenting
    * it using the Setbound pass later on.
    */
  llvm::Value *convertGEP(llvm::Type *sElemType, llvm::Value *pointer);

  bool runOnGEP(llvm::GetElementPtrInst *gep);
  bool updateConsExprs(llvm::Instruction *inst);
};

#endif

/* vim: set et ts=2 sw=2: */
