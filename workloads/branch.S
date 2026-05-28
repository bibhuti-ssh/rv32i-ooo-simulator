    .section .text
    .globl main
main:
    li t0, 0
    li t1, 64
    li t2, 0
1:
    andi t3, t0, 1
    beq t3, zero, 2f
    addi t2, t2, 3
    j 3f
2:
    addi t2, t2, 1
3:
    addi t0, t0, 1
    blt t0, t1, 1b
    li a0, 0
    ret
