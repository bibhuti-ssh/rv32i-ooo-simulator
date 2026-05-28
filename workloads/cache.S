    .section .text
    .globl main
main:
    la t0, buffer
    li t1, 0
    li t2, 256
1:
    sw t1, 0(t0)
    lw t3, 0(t0)
    add t1, t1, t3
    addi t0, t0, 64
    addi t2, t2, -1
    bne t2, zero, 1b
    li a0, 0
    ret

    .section .bss
    .align 6
buffer:
    .space 16384
