override CC = clang
override LD = riscv32-unknown-elf-gcc

# Path to Hardbound LLVM compiler pass.
HARDBOUND ?= LLVMHardbound.so

CFLAGS  += -target riscv32-unknown-elf
CFLAGS  += -march=rv32i -mabi=ilp32
LDFLAGS += -nostartfiles

SOURCES  = $(wildcard *.c)
PROGRAMS = $(SOURCES:.c=)

all: $(PROGRAMS)

%.bc: %.c
	$(CC) -emit-llvm -S -c -o $@ $< $(CFLAGS)
%.ll: %.bc
	opt -load $(HARDBOUND) -array2pointer -setbound 2>/dev/null < $< > $@
%.o: %.ll
	$(CC) -c -o $@ $< $(CFLAGS)
%: bootstrap.o %.o
	$(LD) -o $@ $^ $(LDFLAGS)

bootstrap.o: bootstrap.S
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: all

# Disable all implicit makefile rules
.SUFFIXES:

# Make sure that intermediate files are not deleted
.SECONDARY:
