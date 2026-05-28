    .section .text.start
    .globl _start
_start:
    la sp, _stack_top
    call main
    mv a0, a0
    li a7, 93
    ecall

    .section .bss
    .align 4
_stack:
    .space 4096
_stack_top:
