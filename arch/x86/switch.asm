BITS 32

GLOBAL scheduler_switch_context

; void scheduler_switch_context(uint32_t **old_sp, uint32_t *new_sp)
scheduler_switch_context:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov [eax], esp
    mov esp, [esp + 24]

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
