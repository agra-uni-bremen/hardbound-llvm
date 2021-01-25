override CC = clang
override LD = riscv32-unknown-elf-gcc

# Path to Hardbound LLVM compiler pass.
HARDBOUND ?= LLVMHardbound.so

CFLAGS  += -w -target riscv32-unknown-elf
CFLAGS  += -march=rv32i -mabi=ilp32

CFLAGS  += -Xclang -load
CFLAGS  += -Xclang $(HARDBOUND)

LDFLAGS += -nostartfiles

SOURCES  = $(wildcard *.c)
PROGRAMS = $(SOURCES:.c=)

all: $(PROGRAMS)

%.o: %.c
	clang -c -o $@ $< $(CFLAGS) 2>/dev/null
%: bootstrap.o %.o
	$(LD) -o $@ $^ $(LDFLAGS)

bootstrap.o: bootstrap.S
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: all

# Disable all implicit makefile rules
.SUFFIXES:
