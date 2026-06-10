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
    movl $1, %eax
    movl %eax, t0(%rbp)
    movl $0, %eax
    movl %eax, t1(%rbp)
    movl t0(%rbp), %eax
    movl %eax, arr[t1](%rbp)
    movl $0, %eax
    movl %eax, t2(%rbp)
    movl arr[t2](%rbp), %eax
    movl %eax, t3(%rbp)
    movl t3(%rbp), %eax
    movl %eax, x(%rbp)
    movl x(%rbp), %eax
    movl %eax, t4(%rbp)
    movl t4(%rbp), %eax
    leave
    ret
    leave
    ret
