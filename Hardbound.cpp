#include "Array2Pointer.h"
#include "Setbound.h"

#define DEBUG_TYPE "hardbound"

using namespace llvm;

char Array2Pointer::ID = 0;
static RegisterPass<Array2Pointer> A("array2pointer", "hardbound array2pointer compiler pass");

char Setbound::ID = 0;
static RegisterPass<Setbound> S("setbound", "hardbound setbounds compiler pass");

/* vim: set et ts=2 sw=2: */
