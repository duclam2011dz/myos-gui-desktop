; BIOS boot sector. Loads stage2 from disk and jumps to it.

BITS 16
ORG 0x7C00

STAGE2_SEG equ 0x0800
STAGE2_SECTORS equ 4

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov ax, STAGE2_SEG
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0x00
    mov cl, 0x02
    mov dh, 0x00
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov dl, [boot_drive]
    jmp STAGE2_SEG:0x0000

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

boot_drive db 0
msg_disk_error db "Stage2 load failed", 13, 10, 0

times 510 - ($ - $$) db 0
dw 0xAA55
