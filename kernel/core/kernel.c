#include "heap.h"
#include "bootinfo.h"
#include "diskfs.h"
#include "gdt.h"
#include "graphics.h"
#include "gui.h"
#include "initrd.h"
#include "idt.h"
#include "keyboard.h"
#include "mouse.h"
#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "serial.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"

#define E820_MAGIC 0x534D4150

void kernel_main(uint32_t boot_magic, const struct e820_entry *e820_entries, uint32_t e820_count,
                 const struct boot_graphics_info *boot_graphics)
{
    serial_initialize();
    if (boot_graphics != 0 && boot_graphics->magic == BOOT_GRAPHICS_MAGIC) {
        paging_set_boot_framebuffer(boot_graphics->framebuffer, boot_graphics->pitch * boot_graphics->height);
    }
    graphics_initialize(boot_graphics);
    terminal_initialize();
    paging_initialize();
    gdt_initialize();
    if (boot_magic != E820_MAGIC) {
        e820_entries = 0;
        e820_count = 0;
    }
    pmm_initialize(e820_entries, e820_count);
    heap_initialize();
    scheduler_initialize();
    initrd_initialize();
    diskfs_initialize(FS_START_LBA);
    idt_initialize();
    timer_initialize(100);
    keyboard_initialize();
    mouse_initialize();

    terminal_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("MyOS booted successfully.\n");

    terminal_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Kernel is running in 32-bit protected mode.\n");
    terminal_writestring(paging_is_enabled() ? "Paging enabled.\n" : "Paging disabled.\n");
    terminal_writestring("IDT, PIT timer, keyboard, PMM, heap, and graphics foundation initialized.\n");

    serial_writestring("MyOS serial: kernel_main reached.\n");
    __asm__ volatile ("sti");

    gui_initialize();
    gui_run();
}
