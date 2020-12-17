override CC = clang
override LD = riscv32-unknown-elf-gcc

# Path to Hardbound LLVM compiler pass.
HARDBOUND ?= LLVMHardbound.so

CFLAGS  += -target riscv32-unknown-elf
CFLAGS  += -march=rv32i -mabi=ilp32
LDFLAGS += -nostartfiles

intptr: bootstrap.o intptr.o
bootstrap.o: bootstrap.S
	$(CC) -c -o $@ $< $(CFLAGS)

%.ll: %.c
	$(CC) -emit-llvm -S -c -o $@ $< $(CFLAGS)
%.bc: %.ll
	opt -load $(HARDBOUND) -hardbound < $< > $@
%.o: %.bc
	$(CC) -c -o $@ $< $(CFLAGS)
%: %.o
	$(LD) -o $@ $^ $(LDFLAGS)

.PHONY: all
