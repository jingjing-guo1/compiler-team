.section .text
.global main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $32, %rsp
    movl $10, -8(%rbp)
    movl $20, -12(%rbp)
    movl -8(%rbp), %eax
    movl -12(%rbp), %ecx
    addl %ecx, %eax
    movl %eax, -16(%rbp)
    movl -16(%rbp), %eax
    leave
    ret
