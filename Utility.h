#ifndef HARDBOUND_UTIL
#define HARDBOUND_UTIL

#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"

llvm::Value *xsizeof(llvm::IRBuilder<> *builder, llvm::DataLayout *DL, llvm::Type *type);

#endif
