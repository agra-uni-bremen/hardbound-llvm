#include "Utility.h"

using namespace llvm;

static Value *
getArraySize(IRBuilder<> *builder, DataLayout *DL, Type *type)
{
  if (!type->isArrayTy())
    return nullptr;

  auto elems = type->getArrayNumElements();
  auto elem_size = xsizeof(builder, DL, type->getArrayElementType());

  auto elem_size_const = dyn_cast<ConstantInt>(elem_size);
  if (!elem_size)
    llvm_unreachable("elem_size is not constant");

  size_t total_size = elems * elem_size_const->getZExtValue();
  return builder->getInt32(total_size);
}


Value *
xsizeof(IRBuilder<> *builder, DataLayout *DL, Type *type)
{
  if (type->isArrayTy())
    return getArraySize(builder, DL, type);

  StructType *tstruct = dyn_cast<StructType>(type);
  if (tstruct) {
    const StructLayout *sl = DL->getStructLayout(tstruct);
    return builder->getInt32(sl->getSizeInBytes());
  }

  unsigned size;
  if (type->isPointerTy()) {
    size = DL->getPointerSize();
  } else {
    assert(type->getScalarSizeInBits() != 0);
    size = type->getScalarSizeInBits() / CHAR_BIT;
  }

  return builder->getInt32(size);
}
