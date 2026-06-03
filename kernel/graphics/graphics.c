#include "graphics.h"

static struct graphics_surface primary_surface;
static bool primary_surface_ready;

static void graphics_copy_bytes(uint8_t *dst, const uint8_t *src, uint32_t count)
{
    uint32_t i = 0;
    for (; i + sizeof(uint32_t) <= count; i += sizeof(uint32_t)) {
        *((uint32_t *) (dst + i)) = *((const uint32_t *) (src + i));
    }
    for (; i < count; i++) {
        dst[i] = src[i];
    }
}

static uint32_t palette_color(uint32_t color)
{
    static const uint32_t palette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
    };

    return color < 16 ? palette[color] : color;
}

static uint16_t color_to_rgb565(uint32_t color)
{
    color = palette_color(color);
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    return (uint16_t) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

void graphics_initialize(const struct boot_graphics_info *boot_graphics)
{
    if (boot_graphics != 0 && boot_graphics->magic == BOOT_GRAPHICS_MAGIC &&
        boot_graphics->framebuffer != 0 && boot_graphics->width != 0 &&
        boot_graphics->height != 0 && boot_graphics->pitch != 0) {
        primary_surface.pixels = (uint8_t *) boot_graphics->framebuffer;
        primary_surface.width = boot_graphics->width;
        primary_surface.height = boot_graphics->height;
        primary_surface.pitch = boot_graphics->pitch;
        primary_surface.bits_per_pixel = boot_graphics->bits_per_pixel;
        primary_surface.format = (enum graphics_pixel_format) boot_graphics->pixel_format;
    } else {
        primary_surface.pixels = (uint8_t *) 0xA0000;
        primary_surface.width = 320;
        primary_surface.height = 200;
        primary_surface.pitch = 320;
        primary_surface.bits_per_pixel = 8;
        primary_surface.format = GRAPHICS_PIXEL_FORMAT_INDEXED8;
    }
    primary_surface_ready = true;
}

bool graphics_surface_is_valid(const struct graphics_surface *surface)
{
    if (surface == 0 || surface->pixels == 0 || surface->width == 0 || surface->height == 0) {
        return false;
    }

    uint32_t min_pitch = surface->width * ((surface->bits_per_pixel + 7) / 8);
    if (surface->pitch < min_pitch) {
        return false;
    }

    return surface->bits_per_pixel == 8 || surface->bits_per_pixel == 16 ||
           surface->bits_per_pixel == 24 || surface->bits_per_pixel == 32;
}

bool graphics_primary_available(void)
{
    return primary_surface_ready;
}

const struct graphics_surface *graphics_primary_surface(void)
{
    return primary_surface_ready ? &primary_surface : 0;
}

struct graphics_surface *graphics_primary_surface_mut(void)
{
    return primary_surface_ready ? &primary_surface : 0;
}

void graphics_set_primary_surface(const struct graphics_surface *surface)
{
    if (!graphics_surface_is_valid(surface)) {
        primary_surface_ready = false;
        return;
    }

    primary_surface = *surface;
    primary_surface_ready = true;
}

void graphics_put_pixel(struct graphics_surface *surface, uint32_t x, uint32_t y, uint32_t color)
{
    if (!graphics_surface_is_valid(surface) || x >= surface->width || y >= surface->height) {
        return;
    }

    uint8_t *pixel = surface->pixels + y * surface->pitch + x * ((surface->bits_per_pixel + 7) / 8);
    if (surface->bits_per_pixel == 32) {
        *((uint32_t *) pixel) = palette_color(color);
    } else if (surface->bits_per_pixel == 24) {
        color = palette_color(color);
        pixel[0] = (uint8_t) (color & 0xFF);
        pixel[1] = (uint8_t) ((color >> 8) & 0xFF);
        pixel[2] = (uint8_t) ((color >> 16) & 0xFF);
    } else if (surface->bits_per_pixel == 16) {
        *((uint16_t *) pixel) = color_to_rgb565(color);
    } else {
        *pixel = (uint8_t) color;
    }
}

uint32_t graphics_get_pixel(const struct graphics_surface *surface, uint32_t x, uint32_t y)
{
    if (!graphics_surface_is_valid(surface) || x >= surface->width || y >= surface->height) {
        return 0;
    }

    const uint8_t *pixel = surface->pixels + y * surface->pitch + x * ((surface->bits_per_pixel + 7) / 8);
    if (surface->bits_per_pixel == 32) {
        return *((const uint32_t *) pixel);
    }
    if (surface->bits_per_pixel == 24) {
        return (uint32_t) pixel[0] | ((uint32_t) pixel[1] << 8) | ((uint32_t) pixel[2] << 16);
    }
    if (surface->bits_per_pixel == 16) {
        return *((const uint16_t *) pixel);
    }
    return *pixel;
}

void graphics_clear(struct graphics_surface *surface, uint32_t color)
{
    if (!graphics_surface_is_valid(surface)) {
        return;
    }

    graphics_fill_rect(surface, 0, 0, surface->width, surface->height, color);
}

void graphics_fill_rect(struct graphics_surface *surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    if (!graphics_surface_is_valid(surface) || x >= surface->width || y >= surface->height) {
        return;
    }

    if (x + width > surface->width) {
        width = surface->width - x;
    }
    if (y + height > surface->height) {
        height = surface->height - y;
    }

    uint32_t bytes_per_pixel = (surface->bits_per_pixel + 7) / 8;
    uint32_t mapped = palette_color(color);
    for (uint32_t row = 0; row < height; row++) {
        uint8_t *pixel = surface->pixels + (y + row) * surface->pitch + x * bytes_per_pixel;
        if (surface->bits_per_pixel == 32) {
            uint32_t *dst = (uint32_t *) pixel;
            for (uint32_t col = 0; col < width; col++) {
                dst[col] = mapped;
            }
        } else if (surface->bits_per_pixel == 16) {
            uint16_t mapped16 = color_to_rgb565(color);
            uint16_t *dst = (uint16_t *) pixel;
            for (uint32_t col = 0; col < width; col++) {
                dst[col] = mapped16;
            }
        } else if (surface->bits_per_pixel == 8) {
            for (uint32_t col = 0; col < width; col++) {
                pixel[col] = (uint8_t) color;
            }
        } else {
            for (uint32_t col = 0; col < width; col++) {
                pixel[col * 3] = (uint8_t) (mapped & 0xFF);
                pixel[col * 3 + 1] = (uint8_t) ((mapped >> 8) & 0xFF);
                pixel[col * 3 + 2] = (uint8_t) ((mapped >> 16) & 0xFF);
            }
        }
    }
}

void graphics_draw_rect(struct graphics_surface *surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    if (width == 0 || height == 0) {
        return;
    }

    graphics_fill_rect(surface, x, y, width, 1, color);
    graphics_fill_rect(surface, x, y + height - 1, width, 1, color);
    graphics_fill_rect(surface, x, y, 1, height, color);
    graphics_fill_rect(surface, x + width - 1, y, 1, height, color);
}

void graphics_blit(struct graphics_surface *dst, const struct graphics_surface *src)
{
    if (!graphics_surface_is_valid(dst) || !graphics_surface_is_valid(src)) {
        return;
    }

    uint32_t width = dst->width < src->width ? dst->width : src->width;
    uint32_t height = dst->height < src->height ? dst->height : src->height;
    uint32_t bytes_per_pixel = (dst->bits_per_pixel + 7) / 8;
    if (dst->bits_per_pixel == src->bits_per_pixel && dst->format == src->format) {
        uint32_t row_bytes = width * bytes_per_pixel;
        for (uint32_t y = 0; y < height; y++) {
            uint8_t *dst_row = dst->pixels + y * dst->pitch;
            const uint8_t *src_row = src->pixels + y * src->pitch;
            graphics_copy_bytes(dst_row, src_row, row_bytes);
        }
        return;
    }

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            graphics_put_pixel(dst, x, y, graphics_get_pixel(src, x, y));
        }
    }
}

void graphics_blit_rect(struct graphics_surface *dst, const struct graphics_surface *src,
                        uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!graphics_surface_is_valid(dst) || !graphics_surface_is_valid(src) ||
        x >= dst->width || y >= dst->height || x >= src->width || y >= src->height) {
        return;
    }

    uint32_t max_width = dst->width < src->width ? dst->width : src->width;
    uint32_t max_height = dst->height < src->height ? dst->height : src->height;
    if (x + width > max_width) {
        width = max_width - x;
    }
    if (y + height > max_height) {
        height = max_height - y;
    }

    uint32_t bytes_per_pixel = (dst->bits_per_pixel + 7) / 8;
    if (dst->bits_per_pixel == src->bits_per_pixel && dst->format == src->format) {
        uint32_t row_bytes = width * bytes_per_pixel;
        for (uint32_t row = 0; row < height; row++) {
            uint8_t *dst_row = dst->pixels + (y + row) * dst->pitch + x * bytes_per_pixel;
            const uint8_t *src_row = src->pixels + (y + row) * src->pitch + x * bytes_per_pixel;
            graphics_copy_bytes(dst_row, src_row, row_bytes);
        }
        return;
    }

    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            graphics_put_pixel(dst, x + col, y + row, graphics_get_pixel(src, x + col, y + row));
        }
    }
}

static uint8_t glyph_row(char c, uint32_t row)
{
    if (c >= 'a' && c <= 'z') {
        c = (char) (c - 'a' + 'A');
    }

    static const uint8_t digits[10][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
        {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
        {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
    };

    static const uint8_t letters[26][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C},
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    };

    if (row >= 7) {
        return 0;
    }
    if (c >= '0' && c <= '9') {
        return digits[c - '0'][row];
    }
    if (c >= 'A' && c <= 'Z') {
        return letters[c - 'A'][row];
    }

    switch (c) {
    case ' ': return 0x00;
    case '.': return row == 6 ? 0x04 : 0x00;
    case ',': return row == 5 ? 0x04 : (row == 6 ? 0x08 : 0x00);
    case ':': return row == 2 || row == 5 ? 0x04 : 0x00;
    case ';': return row == 2 ? 0x04 : (row == 5 ? 0x04 : (row == 6 ? 0x08 : 0x00));
    case '-': return row == 3 ? 0x1F : 0x00;
    case '_': return row == 6 ? 0x1F : 0x00;
    case '/': return (uint8_t) (0x01 << (4 - (row > 4 ? 4 : row)));
    case '\\': return (uint8_t) (0x01 << (row > 4 ? 4 : row));
    case '>': return row == 1 ? 0x08 : row == 2 ? 0x04 : row == 3 ? 0x02 : row == 4 ? 0x04 : row == 5 ? 0x08 : 0x00;
    case '<': return row == 1 ? 0x02 : row == 2 ? 0x04 : row == 3 ? 0x08 : row == 4 ? 0x04 : row == 5 ? 0x02 : 0x00;
    case '[': return row == 0 || row == 6 ? 0x0E : 0x08;
    case ']': return row == 0 || row == 6 ? 0x0E : 0x02;
    case '(': return row == 0 ? 0x02 : row == 6 ? 0x02 : 0x04;
    case ')': return row == 0 ? 0x08 : row == 6 ? 0x08 : 0x04;
    case '+': return row == 3 ? 0x0E : row == 2 || row == 4 ? 0x04 : 0x00;
    case '=': return row == 2 || row == 4 ? 0x1F : 0x00;
    case '*': return row == 1 || row == 5 ? 0x15 : row == 2 || row == 4 ? 0x0E : row == 3 ? 0x1F : 0x00;
    case '!': return row < 5 ? 0x04 : row == 6 ? 0x04 : 0x00;
    case '?': return row == 0 ? 0x0E : row == 1 ? 0x11 : row == 2 ? 0x01 : row == 3 ? 0x02 : row == 5 ? 0x04 : 0x00;
    case '\'': return row < 2 ? 0x04 : 0x00;
    case '"': return row < 2 ? 0x0A : 0x00;
    default: return row == 0 || row == 6 ? 0x1F : 0x11;
    }
}

void graphics_draw_char(struct graphics_surface *surface, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg)
{
    for (uint32_t row = 0; row < 8; row++) {
        uint8_t bits = glyph_row(c, row);
        for (uint32_t col = 0; col < 6; col++) {
            uint32_t color = (col < 5 && row < 7 && ((bits >> (4 - col)) & 1) != 0) ? fg : bg;
            graphics_put_pixel(surface, x + col, y + row, color);
        }
    }
}

void graphics_draw_string(struct graphics_surface *surface, uint32_t x, uint32_t y, const char *text, uint32_t fg, uint32_t bg)
{
    uint32_t cursor = x;
    for (uint32_t i = 0; text[i] != '\0'; i++) {
        graphics_draw_char(surface, cursor, y, text[i], fg, bg);
        cursor += 6;
    }
}

bool graphics_self_test(void)
{
    static uint32_t test_pixels[64 * 48];
    struct graphics_surface surface;

    surface.pixels = (uint8_t *) test_pixels;
    surface.width = 64;
    surface.height = 48;
    surface.pitch = 64 * sizeof(uint32_t);
    surface.bits_per_pixel = 32;
    surface.format = GRAPHICS_PIXEL_FORMAT_XRGB8888;

    graphics_clear(&surface, 0x00102030);
    graphics_fill_rect(&surface, 4, 5, 20, 10, 0x00AA3300);
    graphics_draw_rect(&surface, 2, 3, 30, 20, 0x0000CC66);

    return test_pixels[0] == 0x00102030 &&
           test_pixels[5 * 64 + 4] == 0x00AA3300 &&
           test_pixels[3 * 64 + 2] == 0x0000CC66 &&
           test_pixels[22 * 64 + 31] == 0x0000CC66;
}
