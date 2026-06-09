.section .text
.global main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
.globl main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
    movl $0, %eax
    movl %eax, x(%rbp)
    movl x(%rbp), %eax
    movl %eax, t0(%rbp)
    movl t0(%rbp), %eax
    leave
    ret
    movl $0, %eax
    movl %eax, x(%rbp)
    movl x(%rbp), %eax
    movl %eax, t1(%rbp)
    movl t1(%rbp), %eax
    leave
    ret
    movl $0, %eax
    movl %eax, x(%rbp)
    movl x(%rbp), %eax
    movl %eax, t2(%rbp)
    movl t2(%rbp), %eax
    leave
    ret
    movl x(%rbp), %eax
    movl %eax, t3(%rbp)
    movl t3(%rbp), %eax
    leave
    ret
    leave
    ret
