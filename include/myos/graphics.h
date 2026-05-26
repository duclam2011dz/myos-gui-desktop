#ifndef MYOS_GRAPHICS_H
#define MYOS_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"

enum graphics_pixel_format {
    GRAPHICS_PIXEL_FORMAT_INDEXED8,
    GRAPHICS_PIXEL_FORMAT_RGB565,
    GRAPHICS_PIXEL_FORMAT_RGB888,
    GRAPHICS_PIXEL_FORMAT_XRGB8888,
};

struct graphics_surface {
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bits_per_pixel;
    enum graphics_pixel_format format;
};

void graphics_initialize(const struct boot_graphics_info *boot_graphics);
bool graphics_primary_available(void);
const struct graphics_surface *graphics_primary_surface(void);
struct graphics_surface *graphics_primary_surface_mut(void);
void graphics_set_primary_surface(const struct graphics_surface *surface);
bool graphics_surface_is_valid(const struct graphics_surface *surface);
void graphics_put_pixel(struct graphics_surface *surface, uint32_t x, uint32_t y, uint32_t color);
uint32_t graphics_get_pixel(const struct graphics_surface *surface, uint32_t x, uint32_t y);
void graphics_clear(struct graphics_surface *surface, uint32_t color);
void graphics_fill_rect(struct graphics_surface *surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void graphics_draw_rect(struct graphics_surface *surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void graphics_blit(struct graphics_surface *dst, const struct graphics_surface *src);
void graphics_blit_rect(struct graphics_surface *dst, const struct graphics_surface *src,
                        uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void graphics_draw_char(struct graphics_surface *surface, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void graphics_draw_string(struct graphics_surface *surface, uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg);
bool graphics_self_test(void);

#endif
