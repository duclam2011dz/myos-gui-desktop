#include "power.h"

#include "io.h"
#include "serial.h"

#include <stdint.h>

static void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

void power_shutdown(void)
{
    serial_writestring("MyOS power: shutdown requested.\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void power_restart(void)
{
    serial_writestring("MyOS power: restart requested.\n");
    while ((inb(0x64) & 0x02) != 0) {
    }
    outb(0x64, 0xFE);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
