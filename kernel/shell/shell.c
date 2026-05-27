#include "shell.h"

#include "ata.h"
#include "diskfs.h"
#include "graphics.h"
#include "heap.h"
#include "initrd.h"
#include "keyboard.h"
#include "paging.h"
#include "pci.h"
#include "pmm.h"
#include "scheduler.h"
#include "serial.h"
#include "timer.h"
#include "usermode.h"
#include "util.h"
#include "vga.h"

#include "io.h"

#define SHELL_LINE_SIZE 80

static void shell_write(const char *text)
{
    terminal_writestring(text);
    serial_writestring(text);
}

static void shell_prompt(void)
{
    terminal_set_color(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    shell_write("myos> ");
    terminal_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

static void shell_readline(char *buffer, size_t buffer_size)
{
    size_t length = 0;

    for (;;) {
        char c = keyboard_read_char();

        if (c == '\n') {
            shell_write("\n");
            buffer[length] = '\0';
            return;
        }

        if (c == '\b') {
            if (length > 0) {
                length--;
                terminal_putchar('\b');
                serial_write_char('\b');
                serial_write_char(' ');
                serial_write_char('\b');
            }
            continue;
        }

        if (c < ' ' || c > '~') {
            continue;
        }

        if (length + 1 < buffer_size) {
            buffer[length++] = c;
            terminal_putchar(c);
            serial_write_char(c);
        }
    }
}

static void shell_print_hex_line(const char *name, uint32_t value)
{
    char hex[11];
    u32_to_hex(value, hex);
    shell_write(name);
    shell_write(hex);
    shell_write("\n");
}

static void shell_print_u32_line(const char *name, uint32_t value)
{
    char dec[11];
    u32_to_dec(value, dec, sizeof(dec));
    shell_write(name);
    shell_write(dec);
    shell_write("\n");
}

static void shell_print_size_line(const char *name, size_t value)
{
    char dec[21];
    u32_to_dec((uint32_t) value, dec, sizeof(dec));
    shell_write(name);
    shell_write(dec);
    shell_write("\n");
}

static uint32_t parse_u32(const char *text)
{
    uint32_t value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (uint32_t) (*text - '0');
        text++;
    }
    return value;
}

static void shell_command_mem(void)
{
    shell_write(paging_is_enabled() ? "Paging: enabled\n" : "Paging: disabled\n");
    shell_print_u32_line("E820 entries=", pmm_e820_entries());
    shell_print_hex_line("PMM start=", (uint32_t) pmm_managed_start());
    shell_print_hex_line("PMM end=", (uint32_t) pmm_managed_end());
    shell_print_u32_line("PMM total pages=", pmm_total_pages());
    shell_print_u32_line("PMM used pages=", pmm_used_pages());
    shell_print_u32_line("PMM free pages=", pmm_free_pages());
    shell_print_size_line("Heap total bytes=", heap_total_bytes());
    shell_print_size_line("Heap used bytes=", heap_used_bytes());
    shell_print_size_line("Heap free bytes=", heap_free_bytes());
}

static void shell_command_tasks(void)
{
    shell_print_u32_line("Current task=", scheduler_current_task());
    shell_print_u32_line("Context switches=", scheduler_switch_count());

    for (uint32_t i = 0; i < scheduler_task_count(); i++) {
        shell_write("Task ");
        char id[11];
        u32_to_dec(i, id, sizeof(id));
        shell_write(id);
        shell_write(": ");
        shell_write(scheduler_task_name(i));
        shell_write(" ");
        shell_write(scheduler_task_state(i));
        shell_write("\n");
    }
}

static void shell_command_procs(void)
{
    shell_print_u32_line("User processes=", user_process_count());
    for (uint32_t i = 0; i < user_process_count(); i++) {
        shell_write("PID ");
        char value[12];
        u32_to_dec(user_process_pid(i), value, sizeof(value));
        shell_write(value);
        shell_write(": ");
        shell_write(user_process_name(i));
        shell_write(" ");
        shell_write(user_process_state(i));
        shell_write(" exit=");
        u32_to_dec((uint32_t) user_process_exit_code(i), value, sizeof(value));
        shell_write(value);
        shell_write("\n");
    }
}

static void shell_command_wait(const char *pid_text)
{
    uint32_t pid = parse_u32(pid_text);
    int status = user_process_wait_pid(pid);
    if (status == -1) {
        shell_write("Process not found.\n");
        return;
    }
    if (status == -2) {
        shell_write("Process still running.\n");
        return;
    }

    shell_write("Process exit code ");
    char value[12];
    u32_to_dec((uint32_t) status, value, sizeof(value));
    shell_write(value);
    shell_write(".\n");
}

static void shell_command_reap(void)
{
    uint32_t count = user_process_reap_exited();
    shell_print_u32_line("Reaped processes=", count);
}

static void shell_command_disk(void)
{
    uint8_t *sector = (uint8_t *) kmalloc(512);
    if (sector == 0) {
        shell_write("Disk test failed: allocation failed.\n");
        return;
    }

    if (!ata_read_sector(0, sector)) {
        shell_write("Disk test failed: ATA read error.\n");
        kfree(sector);
        return;
    }

    if (sector[510] == 0x55 && sector[511] == 0xAA) {
        shell_write("Disk test passed: boot signature 0xAA55 found.\n");
    } else {
        shell_write("Disk test read sector 0, but boot signature was not found.\n");
    }

    kfree(sector);
}

static void shell_command_ls(void)
{
    if (diskfs_is_available()) {
        shell_write("diskfs:\n");
        for (size_t i = 0; i < diskfs_file_count(); i++) {
            shell_write(diskfs_file_name(i));
            shell_write("\n");
        }
        return;
    }

    shell_write("initrd fallback:\n");
    for (size_t i = 0; i < initrd_file_count(); i++) {
        shell_write(initrd_file_name(i));
        shell_write("\n");
    }
}

static void shell_command_cat(const char *name)
{
    size_t size;
    const char *data = diskfs_is_available() ? diskfs_read_file(name, &size) : 0;
    if (data == 0) {
        data = initrd_read_file(name, &size);
    }
    if (data == 0) {
        shell_write("File not found.\n");
        return;
    }

    terminal_write(data, size);
    serial_writestring(data);
    if (size == 0 || data[size - 1] != '\n') {
        shell_write("\n");
    }
}

static void shell_command_write(const char *args)
{
    char path[32];
    uint32_t path_len = 0;

    while (args[path_len] != '\0' && args[path_len] != ' ' && path_len + 1 < sizeof(path)) {
        path[path_len] = args[path_len];
        path_len++;
    }
    path[path_len] = '\0';

    if (path_len == 0 || args[path_len] != ' ') {
        shell_write("Usage: write <path> <text>\n");
        return;
    }

    const char *text = args + path_len + 1;
    size_t size = kstrlen(text);
    if (size == 0 || !diskfs_write_file(path, text, (uint32_t) size)) {
        shell_write("Write failed.\n");
        return;
    }

    shell_write("Write passed.\n");
}

static void shell_command_delete(const char *path)
{
    shell_write(diskfs_delete_file(path) ? "Delete passed.\n" : "Delete failed.\n");
}

static void shell_command_rename(const char *args)
{
    char old_path[32];
    uint32_t len = 0;
    while (args[len] != '\0' && args[len] != ' ' && len + 1 < sizeof(old_path)) {
        old_path[len] = args[len];
        len++;
    }
    old_path[len] = '\0';
    if (len == 0 || args[len] != ' ') {
        shell_write("Usage: rename <old> <new>\n");
        return;
    }
    shell_write(diskfs_rename_file(old_path, args + len + 1) ? "Rename passed.\n" : "Rename failed.\n");
}

static void shell_command_truncate(const char *args)
{
    char path[32];
    uint32_t len = 0;
    while (args[len] != '\0' && args[len] != ' ' && len + 1 < sizeof(path)) {
        path[len] = args[len];
        len++;
    }
    path[len] = '\0';
    if (len == 0 || args[len] != ' ') {
        shell_write("Usage: truncate <path> <size>\n");
        return;
    }
    shell_write(diskfs_truncate_file(path, parse_u32(args + len + 1)) ? "Truncate passed.\n" : "Truncate failed.\n");
}

static void shell_command_fsck(void)
{
    char report[160];
    uint32_t errors = diskfs_fsck(report, sizeof(report));
    shell_write(report);
    shell_print_u32_line("fsck errors=", errors);
}

static void shell_command_pci(void)
{
    shell_print_u32_line("PCI devices=", pci_device_count());
    for (uint32_t i = 0; i < pci_device_count(); i++) {
        char value[11];
        shell_write("PCI ");
        u32_to_dec(i, value, sizeof(value));
        shell_write(value);
        shell_write(": vendor=");
        u32_to_hex(pci_device_vendor(i), value);
        shell_write(value);
        shell_write(" device=");
        u32_to_hex(pci_device_id(i), value);
        shell_write(value);
        shell_write(" class=");
        u32_to_hex(((uint32_t) pci_device_class(i) << 8) | pci_device_subclass(i), value);
        shell_write(value);
        shell_write("\n");
    }
}

static void shell_command_regs(void)
{
    uint32_t cr0;
    uint32_t cr2;
    uint32_t cr3;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    shell_print_hex_line("CR0=", cr0);
    shell_print_hex_line("CR2=", cr2);
    shell_print_hex_line("CR3=", cr3);
}

static void shell_command_reboot(void)
{
    shell_write("Rebooting...\n");

    while ((inb(0x64) & 0x02) != 0) {
    }
    outb(0x64, 0xFE);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void shell_command_fault(void)
{
    shell_write("Triggering page fault...\n");
    volatile uint32_t *bad_address = (uint32_t *) 0xDEADBEEF;
    *bad_address = 0x12345678;
}

static void shell_command_ticks(void)
{
    shell_print_u32_line("Timer ticks=", timer_ticks());
    shell_print_u32_line("Timer hz=", timer_frequency());
}

static void shell_command_uptime(void)
{
    shell_print_u32_line("Uptime seconds=", timer_uptime_seconds());
}

static void shell_command_gfx(void)
{
    shell_write(graphics_primary_available() ? "Graphics primary framebuffer: available\n" :
                                           "Graphics primary framebuffer: not configured\n");
    shell_write(graphics_self_test() ? "Graphics primitive self-test passed.\n" :
                                      "Graphics primitive self-test failed.\n");
}

static void shell_command_heaptest(void)
{
    char *small = (char *) kmalloc(64);
    char *large = (char *) kmalloc(6000);
    char *aligned = (char *) kmalloc_aligned(128, 4096);
    uint32_t double_frees_before = heap_double_free_count();

    if (small == 0 || large == 0 || aligned == 0) {
        shell_write("Heap test failed: allocation returned null.\n");
        kfree(small);
        kfree(large);
        kfree(aligned);
        return;
    }

    for (size_t i = 0; i < 64; i++) {
        small[i] = (char) ('A' + (i % 26));
    }

    for (size_t i = 0; i < 6000; i++) {
        large[i] = (char) (i & 0xFF);
    }

    if (small[0] != 'A' || small[25] != 'Z' || large[4096] != 0 || ((uint32_t) aligned & 0xFFF) != 0) {
        shell_write("Heap test failed: memory verification mismatch.\n");
    } else {
        shell_write("Heap test passed.\n");
    }

    kfree(small);
    kfree(small);
    kfree(large);
    kfree(aligned);
    if (heap_double_free_count() > double_frees_before) {
        shell_write("Heap hardening detected double free.\n");
    }
    shell_print_size_line("Heap used bytes=", heap_used_bytes());
    shell_print_u32_line("Heap invalid frees=", heap_invalid_free_count());
    shell_print_u32_line("Heap double frees=", heap_double_free_count());
}

static void shell_command_pagingtest(void)
{
    uintptr_t virtual_address = 0x40000000;
    void *page = pmm_alloc_page();

    if (page == 0) {
        shell_write("Paging test failed: PMM allocation failed.\n");
        return;
    }

    if (!paging_map_page(virtual_address, (uintptr_t) page, 0x2)) {
        pmm_free_page(page);
        shell_write("Paging test failed: map_page failed.\n");
        return;
    }

    volatile uint32_t *mapped = (volatile uint32_t *) virtual_address;
    *mapped = 0xBEEFCAFE;

    if (*mapped == 0xBEEFCAFE && paging_get_physical(virtual_address) == (uintptr_t) page) {
        shell_write("Paging test passed.\n");
    } else {
        shell_write("Paging test failed: verification mismatch.\n");
    }

    paging_unmap_page(virtual_address);
    pmm_free_page(page);
}

static void shell_command_usertest(void)
{
    shell_write("Launching ring 3 user test...\n");
    int exit_code = usermode_run_test();
    shell_write("User mode returned exit code ");
    char code[12];
    u32_to_dec((uint32_t) exit_code, code, sizeof(code));
    shell_write(code);
    shell_write(".\n");
}

static void shell_command_run(const char *name)
{
    shell_write("Loading user program from diskfs: ");
    shell_write(name);
    shell_write("\n");
    int exit_code = usermode_run_program(name);
    shell_write("Program exited with code ");
    char code[12];
    u32_to_dec((uint32_t) exit_code, code, sizeof(code));
    shell_write(code);
    shell_write(".\n");
}

static void shell_execute(const char *line)
{
    if (line[0] == '\0') {
        return;
    }

    if (kstrcmp(line, "help") == 0) {
        shell_write("Commands: help clear about mem regs ticks uptime heaptest pagingtest usertest run spawn gfx tasks procs wait reap yield disk ls cat write delete rename truncate fsck pci reboot fault\n");
        return;
    }

    if (kstrcmp(line, "clear") == 0) {
        terminal_clear();
        return;
    }

    if (kstrcmp(line, "about") == 0) {
        shell_write("MyOS: 32-bit x86 educational kernel.\n");
        shell_write("Features: IDT, IRQ keyboard, paging, shell, diagnostics.\n");
        return;
    }

    if (kstrcmp(line, "mem") == 0) {
        shell_command_mem();
        return;
    }

    if (kstrcmp(line, "regs") == 0) {
        shell_command_regs();
        return;
    }

    if (kstrcmp(line, "ticks") == 0) {
        shell_command_ticks();
        return;
    }

    if (kstrcmp(line, "uptime") == 0) {
        shell_command_uptime();
        return;
    }

    if (kstrcmp(line, "heaptest") == 0) {
        shell_command_heaptest();
        return;
    }

    if (kstrcmp(line, "pagingtest") == 0) {
        shell_command_pagingtest();
        return;
    }

    if (kstrcmp(line, "usertest") == 0) {
        shell_command_usertest();
        return;
    }

    if (line[0] == 'r' && line[1] == 'u' && line[2] == 'n' && line[3] == ' ') {
        shell_command_run(line + 4);
        return;
    }

    if (line[0] == 's' && line[1] == 'p' && line[2] == 'a' && line[3] == 'w' && line[4] == 'n' && line[5] == ' ') {
        shell_command_run(line + 6);
        return;
    }

    if (kstrcmp(line, "gfx") == 0) {
        shell_command_gfx();
        return;
    }

    if (kstrcmp(line, "tasks") == 0) {
        shell_command_tasks();
        return;
    }

    if (kstrcmp(line, "procs") == 0) {
        shell_command_procs();
        return;
    }

    if (line[0] == 'w' && line[1] == 'a' && line[2] == 'i' && line[3] == 't' && line[4] == ' ') {
        shell_command_wait(line + 5);
        return;
    }

    if (kstrcmp(line, "reap") == 0) {
        shell_command_reap();
        return;
    }

    if (kstrcmp(line, "yield") == 0) {
        scheduler_yield();
        shell_command_tasks();
        return;
    }

    if (kstrcmp(line, "disk") == 0) {
        shell_command_disk();
        return;
    }

    if (kstrcmp(line, "ls") == 0) {
        shell_command_ls();
        return;
    }

    if (line[0] == 'c' && line[1] == 'a' && line[2] == 't' && line[3] == ' ') {
        shell_command_cat(line + 4);
        return;
    }

    if (line[0] == 'w' && line[1] == 'r' && line[2] == 'i' && line[3] == 't' && line[4] == 'e' && line[5] == ' ') {
        shell_command_write(line + 6);
        return;
    }

    if (line[0] == 'd' && line[1] == 'e' && line[2] == 'l' && line[3] == 'e' && line[4] == 't' && line[5] == 'e' && line[6] == ' ') {
        shell_command_delete(line + 7);
        return;
    }

    if (line[0] == 'r' && line[1] == 'e' && line[2] == 'n' && line[3] == 'a' && line[4] == 'm' && line[5] == 'e' && line[6] == ' ') {
        shell_command_rename(line + 7);
        return;
    }

    if (line[0] == 't' && line[1] == 'r' && line[2] == 'u' && line[3] == 'n' && line[4] == 'c' && line[5] == 'a' && line[6] == 't' && line[7] == 'e' && line[8] == ' ') {
        shell_command_truncate(line + 9);
        return;
    }

    if (kstrcmp(line, "fsck") == 0) {
        shell_command_fsck();
        return;
    }

    if (kstrcmp(line, "pci") == 0) {
        shell_command_pci();
        return;
    }

    if (kstrcmp(line, "reboot") == 0) {
        shell_command_reboot();
        return;
    }

    if (kstrcmp(line, "fault") == 0) {
        shell_command_fault();
        return;
    }

    shell_write("Unknown command. Type 'help'.\n");
}

void shell_run(void)
{
    char line[SHELL_LINE_SIZE];

    shell_write("MyOS shell ready. Type 'help'.\n");

    for (;;) {
        shell_prompt();
        shell_readline(line, sizeof(line));
        shell_execute(line);
    }
}
