    .section .text
    .globl  __xthread_start
    .p2align 4
__xthread_start:
    popq    %rsi
    popq    %rdi
    popq    %rbp
    callq   *%rsi
    movq    %rax, %rdi
    jmp     *%rbp
