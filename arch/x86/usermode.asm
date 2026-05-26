BITS 32

GLOBAL usermode_enter
GLOBAL usermode_return_from_exit
GLOBAL usermode_saved_esp
GLOBAL usermode_exit_code

EXTERN tss_set_kernel_stack

; int usermode_enter(uint32_t entry, uint32_t user_stack)
usermode_enter:
    mov [usermode_saved_esp], esp

    mov eax, kernel_syscall_stack_top
    push eax
    call tss_set_kernel_stack
    add esp, 4

    mov eax, [esp + 4]
    mov edx, [esp + 8]

    cli
    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push dword 0x23
    push edx
    pushfd
    or dword [esp], 0x200
    push dword 0x1B
    push eax
    iretd

usermode_return_from_exit:
    mov esp, [usermode_saved_esp]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, [usermode_exit_code]
    ret

SECTION .bss
ALIGN 16
kernel_syscall_stack:
    resb 8192
kernel_syscall_stack_top:

usermode_saved_esp:
    resd 1
usermode_exit_code:
    resd 1
