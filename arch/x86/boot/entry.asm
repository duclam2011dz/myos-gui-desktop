BITS 32

GLOBAL _start
EXTERN kernel_main

SECTION .text
_start:
    mov esp, stack_top
    push edx
    push ecx
    push ebx
    push eax
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

SECTION .bss
ALIGN 16
stack_bottom:
    resb 16384
stack_top:
