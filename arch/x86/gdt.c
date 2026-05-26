#include "gdt.h"

#include <stdint.h>

#define GDT_ENTRIES 6
#define TSS_STACK_SIZE 4096

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

extern void gdt_flush(struct gdt_ptr *ptr);
extern void tss_flush(void);

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_descriptor;
static struct tss_entry tss;
static uint8_t tss_stack[TSS_STACK_SIZE] __attribute__((aligned(16)));

static void gdt_set_gate(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity)
{
    gdt[index].base_low = (uint16_t) (base & 0xFFFF);
    gdt[index].base_middle = (uint8_t) ((base >> 16) & 0xFF);
    gdt[index].base_high = (uint8_t) ((base >> 24) & 0xFF);
    gdt[index].limit_low = (uint16_t) (limit & 0xFFFF);
    gdt[index].granularity = (uint8_t) ((limit >> 16) & 0x0F);
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access = access;
}

static void tss_initialize(void)
{
    uint8_t *raw = (uint8_t *) &tss;
    for (uint32_t i = 0; i < sizeof(tss); i++) {
        raw[i] = 0;
    }

    tss.ss0 = KERNEL_DATA_SELECTOR;
    tss.esp0 = (uint32_t) (tss_stack + TSS_STACK_SIZE);
    tss.cs = USER_CODE_SELECTOR;
    tss.ss = USER_DATA_SELECTOR;
    tss.ds = USER_DATA_SELECTOR;
    tss.es = USER_DATA_SELECTOR;
    tss.fs = USER_DATA_SELECTOR;
    tss.gs = USER_DATA_SELECTOR;
    tss.iomap_base = sizeof(tss);
}

void tss_set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}

void gdt_initialize(void)
{
    gdt_descriptor.limit = sizeof(gdt) - 1;
    gdt_descriptor.base = (uint32_t) &gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);

    tss_initialize();
    gdt_set_gate(5, (uint32_t) &tss, sizeof(tss) - 1, 0x89, 0x40);

    gdt_flush(&gdt_descriptor);
    tss_flush();
}
