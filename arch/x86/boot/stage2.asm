; Stage2 loader. Collects BIOS E820 map, loads the kernel, enters protected mode.

BITS 16
ORG 0x0000

KERNEL_LOAD_SEG equ 0x1000
KERNEL_SECTORS equ 192
KERNEL_START_LBA equ 5
E820_COUNT_ADDR equ 0x4FF0
E820_MAP_SEG equ 0x0500
E820_MAP_OFF equ 0x0000
E820_ENTRY_SIZE equ 24
E820_MAGIC equ 0x534D4150
VBE_MODE_INFO_ADDR equ 0x6000
BOOT_GFX_ADDR equ 0x7000
BOOT_GFX_MAGIC equ 0x31465847

start:
    cli
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl
    mov si, msg_loading
    call print_string

    call collect_e820
    call load_kernel
    call set_graphics_mode
    call enable_a20

    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:(protected_mode + 0x8000)

collect_e820:
    xor ebx, ebx
    xor bp, bp
    mov ax, E820_MAP_SEG
    mov es, ax
    xor di, di

.next:
    mov eax, 0xE820
    mov edx, E820_MAGIC
    mov ecx, E820_ENTRY_SIZE
    int 0x15
    jc .done
    cmp eax, E820_MAGIC
    jne .done
    cmp ecx, 20
    jb .done
    inc bp
    add di, E820_ENTRY_SIZE
    cmp bp, 32
    jae .done
    test ebx, ebx
    jne .next

.done:
    xor ax, ax
    mov es, ax
    mov [es:E820_COUNT_ADDR], bp
    ret

load_kernel:
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
    mov si, KERNEL_SECTORS
    mov di, KERNEL_START_LBA

.read_next:
    push bx
    mov ax, di
    mov bl, 63
    div bl
    mov cl, ah
    inc cl
    mov dh, al
    and dh, 0x0F
    mov ch, al
    shr ch, 4
    pop bx

    mov ah, 0x02
    mov al, 0x01
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    add bx, 512
    jnc .same_segment
    mov ax, es
    add ax, 0x1000
    mov es, ax
.same_segment:
    inc di
    dec si
    jnz .read_next
    ret

enable_a20:
    in al, 0x92
    or al, 0x02
    out 0x92, al
    ret

set_graphics_mode:
    mov si, vbe_modes
.try_vbe:
    lodsw
    test ax, ax
    jz .fallback_vga
    mov [candidate_mode], ax

    push ds
    xor ax, ax
    mov es, ax
    mov di, VBE_MODE_INFO_ADDR
    mov ax, 0x4F01
    mov cx, [candidate_mode]
    int 0x10
    pop ds
    cmp ax, 0x004F
    jne .try_vbe

    push ds
    xor ax, ax
    mov ds, ax
    mov ax, [VBE_MODE_INFO_ADDR]
    test ax, 0x0001
    jz .bad_vbe_info
    test ax, 0x0010
    jz .bad_vbe_info
    test ax, 0x0080
    jz .bad_vbe_info
    cmp dword [VBE_MODE_INFO_ADDR + 40], 0
    je .bad_vbe_info
    mov al, [VBE_MODE_INFO_ADDR + 25]
    cmp al, 8
    je .good_vbe_info
    cmp al, 16
    je .good_vbe_info
    cmp al, 24
    je .good_vbe_info
    cmp al, 32
    je .good_vbe_info
.bad_vbe_info:
    pop ds
    jmp .try_vbe

.good_vbe_info:
    pop ds
    mov ax, 0x4F02
    mov bx, [candidate_mode]
    or bx, 0x4000
    int 0x10
    cmp ax, 0x004F
    jne .try_vbe
    call store_vbe_graphics_info
    ret

.fallback_vga:
    mov ax, 0x0013
    int 0x10
    call store_vga_graphics_info
    ret

store_vbe_graphics_info:
    push ds
    xor ax, ax
    mov ds, ax
    mov dword [BOOT_GFX_ADDR], BOOT_GFX_MAGIC
    mov eax, [VBE_MODE_INFO_ADDR + 40]
    mov [BOOT_GFX_ADDR + 4], eax
    movzx eax, word [VBE_MODE_INFO_ADDR + 16]
    mov [BOOT_GFX_ADDR + 8], eax
    mov ax, [VBE_MODE_INFO_ADDR + 18]
    mov [BOOT_GFX_ADDR + 12], ax
    mov ax, [VBE_MODE_INFO_ADDR + 20]
    mov [BOOT_GFX_ADDR + 14], ax
    mov al, [VBE_MODE_INFO_ADDR + 25]
    mov [BOOT_GFX_ADDR + 16], al
    call vbe_format_from_bpp
    mov [BOOT_GFX_ADDR + 17], al
    mov ax, [candidate_mode]
    mov [BOOT_GFX_ADDR + 18], ax
    pop ds
    ret

store_vga_graphics_info:
    push ds
    xor ax, ax
    mov ds, ax
    mov dword [BOOT_GFX_ADDR], BOOT_GFX_MAGIC
    mov dword [BOOT_GFX_ADDR + 4], 0x000A0000
    mov dword [BOOT_GFX_ADDR + 8], 320
    mov word [BOOT_GFX_ADDR + 12], 320
    mov word [BOOT_GFX_ADDR + 14], 200
    mov byte [BOOT_GFX_ADDR + 16], 8
    mov byte [BOOT_GFX_ADDR + 17], 0
    mov word [BOOT_GFX_ADDR + 18], 0x0013
    pop ds
    ret

vbe_format_from_bpp:
    cmp al, 16
    je .rgb565
    cmp al, 24
    je .rgb888
    cmp al, 32
    je .xrgb8888
    xor al, al
    ret
.rgb565:
    mov al, 1
    ret
.rgb888:
    mov al, 2
    ret
.xrgb8888:
    mov al, 3
    ret

disk_error:
    mov si, msg_disk_error
    call print_string
    hlt
    jmp $

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp print_string
.done:
    ret

BITS 32
protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    mov eax, E820_MAGIC
    mov ebx, 0x00005000
    xor ecx, ecx
    mov cx, [E820_COUNT_ADDR]
    mov edx, 0x00007000
    mov esi, 0x10000
    jmp esi

gdt_start:
    dq 0x0000000000000000
gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start + 0x8000

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

boot_drive db 0
candidate_mode dw 0
vbe_modes dw 0x112, 0x111, 0x101, 0
msg_loading db "Loading MyOS stage2...", 13, 10, 0
msg_disk_error db "Kernel load failed", 13, 10, 0

times (4 * 512) - ($ - $$) db 0
