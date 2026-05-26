ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    _kernel_start = .;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4096);
    _kernel_end = .;
}
