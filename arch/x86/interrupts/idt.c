#include "idt.h"

#include "keyboard.h"
#include "mouse.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall.h"
#include "timer.h"
#include "util.h"
#include "vga.h"

#include "io.h"

#define IDT_ENTRIES 256
#define IDT_KERNEL_CODE_SELECTOR 0x08
#define IDT_INTERRUPT_GATE 0x8E
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t always_zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void idt_load(struct idt_ptr *idt_ptr);

#define ISR_EXTERN(n) extern void isr##n(void)
ISR_EXTERN(0); ISR_EXTERN(1); ISR_EXTERN(2); ISR_EXTERN(3);
ISR_EXTERN(4); ISR_EXTERN(5); ISR_EXTERN(6); ISR_EXTERN(7);
ISR_EXTERN(8); ISR_EXTERN(9); ISR_EXTERN(10); ISR_EXTERN(11);
ISR_EXTERN(12); ISR_EXTERN(13); ISR_EXTERN(14); ISR_EXTERN(15);
ISR_EXTERN(16); ISR_EXTERN(17); ISR_EXTERN(18); ISR_EXTERN(19);
ISR_EXTERN(20); ISR_EXTERN(21); ISR_EXTERN(22); ISR_EXTERN(23);
ISR_EXTERN(24); ISR_EXTERN(25); ISR_EXTERN(26); ISR_EXTERN(27);
ISR_EXTERN(28); ISR_EXTERN(29); ISR_EXTERN(30); ISR_EXTERN(31);
ISR_EXTERN(32); ISR_EXTERN(33); ISR_EXTERN(34); ISR_EXTERN(35);
ISR_EXTERN(36); ISR_EXTERN(37); ISR_EXTERN(38); ISR_EXTERN(39);
ISR_EXTERN(40); ISR_EXTERN(41); ISR_EXTERN(42); ISR_EXTERN(43);
ISR_EXTERN(44); ISR_EXTERN(45); ISR_EXTERN(46); ISR_EXTERN(47);
ISR_EXTERN(128);

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_descriptor;

static const char *exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved",
};

static void idt_set_gate(uint8_t index, uint32_t base, uint16_t selector, uint8_t flags)
{
    idt[index].base_low = (uint16_t) (base & 0xFFFF);
    idt[index].selector = selector;
    idt[index].always_zero = 0;
    idt[index].flags = flags;
    idt[index].base_high = (uint16_t) ((base >> 16) & 0xFFFF);
}

static void pic_remap(void)
{
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

static void pic_send_eoi(uint32_t int_no)
{
    if (int_no >= 40) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    if (int_no >= 32) {
        outb(PIC1_COMMAND, PIC_EOI);
    }
}

static void irq_set_mask(uint8_t irq_line)
{
    uint16_t port = irq_line < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t line = irq_line < 8 ? irq_line : irq_line - 8;
    outb(port, inb(port) | (1 << line));
}

static void irq_clear_mask(uint8_t irq_line)
{
    uint16_t port = irq_line < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t line = irq_line < 8 ? irq_line : irq_line - 8;
    outb(port, inb(port) & ~(1 << line));
}

static void diagnostics_write(const char *text)
{
    terminal_writestring(text);
    serial_writestring(text);
}

static void diagnostics_write_hex(const char *name, uint32_t value)
{
    char hex[11];
    u32_to_hex(value, hex);
    diagnostics_write(name);
    diagnostics_write(hex);
    diagnostics_write("\n");
}

static uint32_t read_cr2(void)
{
    uint32_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static void print_exception_diagnostics(struct interrupt_frame *frame)
{
    const char *name = frame->int_no < 32 ? exception_names[frame->int_no] : "Unknown";

    terminal_set_color(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    diagnostics_write("\nKERNEL PANIC: CPU exception\n");
    diagnostics_write("Exception: ");
    diagnostics_write(name);
    diagnostics_write("\n");

    diagnostics_write_hex("INT=", frame->int_no);
    diagnostics_write_hex("ERR=", frame->err_code);
    diagnostics_write_hex("EIP=", frame->eip);
    diagnostics_write_hex("CS=", frame->cs);
    diagnostics_write_hex("EFLAGS=", frame->eflags);

    if (frame->int_no == 14) {
        diagnostics_write_hex("CR2=", read_cr2());
    }

    diagnostics_write_hex("EAX=", frame->eax);
    diagnostics_write_hex("EBX=", frame->ebx);
    diagnostics_write_hex("ECX=", frame->ecx);
    diagnostics_write_hex("EDX=", frame->edx);
    diagnostics_write_hex("ESI=", frame->esi);
    diagnostics_write_hex("EDI=", frame->edi);
    diagnostics_write_hex("EBP=", frame->ebp);
    diagnostics_write_hex("ESP=", frame->esp);
    diagnostics_write("System halted.\n");
}

void idt_initialize(void)
{
    for (uint16_t i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t) i, 0, IDT_KERNEL_CODE_SELECTOR, 0);
    }

    void (*handlers[48])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
        isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
        isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47,
    };

    for (uint8_t i = 0; i < 48; i++) {
        idt_set_gate(i, (uint32_t) handlers[i], IDT_KERNEL_CODE_SELECTOR, IDT_INTERRUPT_GATE);
    }
    idt_set_gate(128, (uint32_t) isr128, IDT_KERNEL_CODE_SELECTOR, 0xEE);

    pic_remap();

    for (uint8_t irq = 0; irq < 16; irq++) {
        irq_set_mask(irq);
    }
    irq_clear_mask(0);
    irq_clear_mask(1);
    irq_clear_mask(2);
    irq_clear_mask(12);

    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base = (uint32_t) &idt;
    idt_load(&idt_descriptor);
}

struct interrupt_frame *interrupt_dispatch(struct interrupt_frame *frame)
{
    if (frame->int_no == 128) {
        syscall_handle(frame);
        return frame;
    }

    if (frame->int_no == 33) {
        keyboard_handle_interrupt();
        pic_send_eoi(frame->int_no);
        return frame;
    }

    if (frame->int_no == 44) {
        mouse_handle_interrupt();
        pic_send_eoi(frame->int_no);
        return frame;
    }

    if (frame->int_no == 32) {
        timer_handle_interrupt();
        pic_send_eoi(frame->int_no);
        return scheduler_schedule_from_interrupt(frame);
    }

    if (frame->int_no >= 32 && frame->int_no <= 47) {
        pic_send_eoi(frame->int_no);
        return frame;
    }

    print_exception_diagnostics(frame);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}
