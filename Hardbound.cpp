#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "Array2Pointer.h"
#include "Setbound.h"

using namespace llvm;

char Array2Pointer::ID = 0;
static RegisterPass<Array2Pointer> A("array2pointer", "hardbound array2pointer compiler pass");

char Setbound::ID = 0;
static RegisterPass<Setbound> S("setbound", "hardbound setbounds compiler pass");

static void registerHardboundPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
  PM.add(new Array2Pointer());
  PM.add(new Setbound());
}

static llvm::RegisterStandardPasses Y(
  llvm::PassManagerBuilder::EP_FullLinkTimeOptimizationEarly,
  registerHardboundPass
);

/* vim: set et ts=2 sw=2: */
