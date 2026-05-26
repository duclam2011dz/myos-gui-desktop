#include "serial.h"

#include <stddef.h>

#include "io.h"

#define COM1 0x3F8

void serial_initialize(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write_char(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0) {
    }

    outb(COM1, (uint8_t) c);
}

void serial_writestring(const char *data)
{
    for (size_t i = 0; data[i] != '\0'; i++) {
        if (data[i] == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(data[i]);
    }
}
