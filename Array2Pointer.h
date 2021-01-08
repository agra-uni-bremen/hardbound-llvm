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

using namespace llvm;

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

  bool runOnFunction(Function &F) override;

  private:

  void shouldBeInBounds(Value *value);
  Value *getElemPtrIndex(GetElementPtrInst *instr);

  Value *getArrayPointer(IRBuilder<> &builder, Value *array, ArrayType *arrayTy, Value *index);
  LoadInst *transformArrayAccess(IRBuilder<> &builder, GetElementPtrInst *gep, ArrayType *arrayTy);

  Instruction *runOnLoadInstr(IRBuilder<> &builder, LoadInst *loadInst);
  Instruction *runOnCallInst(IRBuilder<> &builder, CallInst *callInst);
};

#endif

/* vim: set et ts=2 sw=2: */
