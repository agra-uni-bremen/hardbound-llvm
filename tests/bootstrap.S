.macro SYS_EXIT, exit_code
li   a7, 93 /* SYS_exit */
li   a0, \exit_code
ecall
.endm

irq_handler:
	li a6, 0x2342 /* success pattern */
	SYS_EXIT 1

.globl _start
.globl main

_start:
/* setup trap handler for load fault */
la t0, irq_handler
csrw mtvec, t0

jal main
SYS_EXIT 0 /* unreachable */
