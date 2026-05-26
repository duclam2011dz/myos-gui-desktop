BITS 32

GLOBAL idt_load
EXTERN interrupt_dispatch

%macro ISR_NOERR 1
GLOBAL isr%1
isr%1:
    cli
    push dword 0
    push dword %1
    jmp interrupt_common
%endmacro

%macro ISR_ERR 1
GLOBAL isr%1
isr%1:
    cli
    push dword %1
    jmp interrupt_common
%endmacro

%assign i 0
%rep 8
ISR_NOERR i
%assign i i+1
%endrep

ISR_ERR 8
ISR_NOERR 9
ISR_ERR 10
ISR_ERR 11
ISR_ERR 12
ISR_ERR 13
ISR_ERR 14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR 17

%assign i 18
%rep 14
ISR_NOERR i
%assign i i+1
%endrep

%assign i 32
%rep 16
ISR_NOERR i
%assign i i+1
%endrep

ISR_NOERR 128

interrupt_common:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_dispatch
    add esp, 4
    mov esp, eax

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret
