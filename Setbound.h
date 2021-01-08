#ifndef HARDBOUND_SETBOUND
#define HARDBOUND_SETBOUND

#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/DerivedTypes.h"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

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
  bool runOnFunction(Function &F) override;

private:

  Instruction *buildSetbound(IRBuilder<> &builder, Value *pointer, Value *base, Value *numbytes);
  Instruction *runOnStoreInstr(IRBuilder<> &builder, StoreInst &storeInst);

  Value *xsizeof(IRBuilder<> &builder, Type *type);
  Value *getArraySize(IRBuilder<> &builder, Type *type);

  Value *baseOffset(IRBuilder<> &builder, Value *offset, Type *source);
  Value *baseOffset(IRBuilder<> &builder, const GetElementPtrInst *instr);

  Value *getValueByteSize(IRBuilder<> &builder, Value *value);
};

#endif

/* vim: set et ts=2 sw=2: */
