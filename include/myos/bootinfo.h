#ifndef MYOS_BOOTINFO_H
#define MYOS_BOOTINFO_H

#include <stdint.h>

#define BOOT_GRAPHICS_MAGIC 0x31465847

struct boot_graphics_info {
    uint32_t magic;
    uint32_t framebuffer;
    uint32_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t bits_per_pixel;
    uint8_t pixel_format;
    uint16_t mode;
} __attribute__((packed));

#endif
