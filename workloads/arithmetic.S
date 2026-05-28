    .section .text
    .globl main
main:
    li t0, 0
    li t1, 100
    li t2, 0
1:
    add t2, t2, t0
    addi t0, t0, 1
    blt t0, t1, 1b
    li a0, 0
    ret
