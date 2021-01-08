#include <assert.h>
#include <stdint.h>

#include "Setbound.h"

#define SETBOUND_ASM  \
  "li x17, 98\n"      \
  "mv x10, $0\n"      \
  "mv x11, $1\n"      \
  "mv x12, $2\n"      \
  "ecall"
#define SETBOUND_CONS \
  "r,r,r,~{x17},~{x10},~{x11},~{x12}"

bool
Setbound::runOnFunction(Function &F)
{
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

Instruction *
Setbound::buildSetbound(IRBuilder<> &builder, Value *pointer, Value *base, Value *numbytes)
{
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

Instruction *
Setbound::runOnStoreInstr(IRBuilder<> &builder, StoreInst &storeInst)
{
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

Value *
Setbound::xsizeof(IRBuilder<> &builder, Type *type)
{
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

Value *
Setbound::getArraySize(IRBuilder<> &builder, Type *type)
{
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

Value *
Setbound::baseOffset(IRBuilder<> &builder, Value *offset, Type *source)
{
  if (!source->isArrayTy())
    llvm_unreachable("getelementptr on non array type");

  auto elemType = source->getArrayElementType();
  auto elemSize = xsizeof(builder, elemType);

  return builder.CreateMul(offset, elemSize);
}

Value *
Setbound::baseOffset(IRBuilder<> &builder, const GetElementPtrInst *instr)
{
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

Value *
Setbound::getValueByteSize(IRBuilder<> &builder, Value *value)
{
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

/* vim: set et ts=2 sw=2: */
