.section .text
.global main
factorial:
    pushq %rbp
    movq %rsp, %rbp
    subq $64, %rsp
    movl %edi, %eax
    movl %eax, -8(%rbp)
    movl $1, -12(%rbp)
L0:
    movl -8(%rbp), %eax
    movl $1, %ecx
    cmpl %ecx, %eax
    setg %al
    movzbl %al, %eax
    movl %eax, -20(%rbp)
    movl -20(%rbp), %eax
    cmpl $0, %eax
    je L1
    movl -12(%rbp), %eax
    movl -8(%rbp), %ecx
    imull %ecx, %eax
    movl %eax, -28(%rbp)
    movl -28(%rbp), %eax
    movl %eax, -12(%rbp)
    movl -8(%rbp), %eax
    movl $1, %ecx
    subl %ecx, %eax
    movl %eax, -32(%rbp)
    movl -32(%rbp), %eax
    movl %eax, -8(%rbp)
    jmp L0
L1:
    movl -12(%rbp), %eax
    leave
    ret
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $64, %rsp
    movl $5, -40(%rbp)
    movl -40(%rbp), %eax
    movq %rax, %rdi
    call factorial
    movl %eax, -44(%rbp)
    movl -44(%rbp), %eax
    movl %eax, -48(%rbp)
    movl -48(%rbp), %eax
    leave
    ret
