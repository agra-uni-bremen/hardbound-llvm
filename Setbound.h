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
 * This allows bounds-checking to be peformed in the Hardware as
 * described in the original Hardbound paper.
 */
struct Setbound : public llvm::FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  llvm::LLVMContext context;
  llvm::DataLayout *DL;

  Setbound() : FunctionPass(ID) {}
  bool runOnFunction(llvm::Function &F) override;

private:

  llvm::Instruction *buildSetbound(llvm::IRBuilder<> &builder, llvm::Value *pointer, llvm::Value *base, llvm::Value *numbytes);
  llvm::Instruction *runOnStoreInstr(llvm::IRBuilder<> &builder, llvm::StoreInst &storeInst);

  llvm::Value *xsizeof(llvm::IRBuilder<> &builder, llvm::Type *type);
  llvm::Value *getArraySize(llvm::IRBuilder<> &builder, llvm::Type *type);

  llvm::Value *baseOffset(llvm::IRBuilder<> &builder, llvm::Value *offset, llvm::Type *source);
  llvm::Value *baseOffset(llvm::IRBuilder<> &builder, const llvm::GetElementPtrInst *instr);

  llvm::Value *getValueByteSize(llvm::IRBuilder<> &builder, llvm::Value *value);
};

#endif

/* vim: set et ts=2 sw=2: */
