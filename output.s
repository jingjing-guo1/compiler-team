.section .text
.global main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
.globl factorial
factorial:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
    movl $1, %eax
    movl %eax, t0(%rbp)
    movl t0(%rbp), %eax
    movl %eax, result(%rbp)
L0:
    movl n(%rbp), %eax
    movl %eax, t1(%rbp)
    movl $1, %eax
    movl %eax, t2(%rbp)
    # unimplemented IR op 8
    cmpl $0, t3(%rbp)
    je L1
    jmp L0
    movl result(%rbp), %eax
    movl %eax, t4(%rbp)
    movl n(%rbp), %eax
    movl %eax, t5(%rbp)
    movl t4(%rbp), %eax
    imull t5(%rbp), %eax
    movl %eax, t6(%rbp)
    movl t6(%rbp), %eax
    movl %eax, result(%rbp)
    movl n(%rbp), %eax
    movl %eax, t7(%rbp)
    movl $1, %eax
    movl %eax, t8(%rbp)
    movl t7(%rbp), %eax
    subl t8(%rbp), %eax
    movl %eax, t9(%rbp)
    movl t9(%rbp), %eax
    movl %eax, n(%rbp)
    jmp L0
L1:
    movl result(%rbp), %eax
    movl %eax, t10(%rbp)
    movl t10(%rbp), %eax
    leave
    ret
    leave
    ret
.globl main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
    movl $5, %eax
    movl %eax, t11(%rbp)
    movl t11(%rbp), %eax
    movl %eax, x(%rbp)
    movl x(%rbp), %eax
    movl %eax, t12(%rbp)
    # unimplemented IR op 19
    movl t13(%rbp), %eax
    movl %eax, y(%rbp)
    movl y(%rbp), %eax
    movl %eax, t14(%rbp)
    movl t14(%rbp), %eax
    leave
    ret
    leave
    ret
