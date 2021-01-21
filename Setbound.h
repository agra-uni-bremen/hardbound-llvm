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

/**
 * Setbound compiler pass.
 *
 * This compiler pass instruments store instructions, inserting a
 * setbound instruction for each store which represents a pointer.
 * This allows bounds-checking to be peformed in the hardware as
 * described in the original Hardbound paper.
 */
struct Setbound : public llvm::FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  llvm::IRBuilder<> *builder;
  llvm::DataLayout *DL;

  Setbound() : FunctionPass(ID) {}
  bool runOnFunction(llvm::Function &F) override;

private:

  llvm::Instruction *buildSetbound(llvm::Value *pointer, llvm::Value *base, llvm::Value *numbytes);
  llvm::Instruction *runOnStoreInstr(llvm::StoreInst &storeInst);

  bool isInstrumented(llvm::Value *value);
  llvm::Value *getValueByteSize(llvm::Value *value);
};

#endif

/* vim: set et ts=2 sw=2: */
