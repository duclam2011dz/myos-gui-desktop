#ifndef MYOS_GDT_H
#define MYOS_GDT_H

#include <stdint.h>

#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR 0x1B
#define USER_DATA_SELECTOR 0x23

void gdt_initialize(void);
void tss_set_kernel_stack(uint32_t stack);

#endif
