#include "assets.h"

#include "diskfs.h"
#include "heap.h"

#define MYIMG_MAGIC 0x4D49594D
#define MYIMG_RGB565 1
#define MYIMG_ARGB32 2

struct myimg_header {
    uint32_t magic;
    uint16_t width;
    uint16_t height;
    uint16_t format;
    uint16_t hotspot_x;
    uint16_t hotspot_y;
    uint16_t reserved;
    uint32_t payload_size;
} __attribute__((packed));

struct asset_bitmap {
    uint16_t width;
    uint16_t height;
    uint16_t format;
    uint16_t hotspot_x;
    uint16_t hotspot_y;
    uint32_t payload_size;
    uint8_t *pixels;
};

static struct asset_bitmap wallpaper;
static struct asset_bitmap cursor;

static bool load_myimg(const char *path, struct asset_bitmap *out)
{
    int index = diskfs_find_file(path);
    if (index < 0) {
        return false;
    }
    struct myimg_header header;
    uint32_t bytes = 0;
    if (!diskfs_read_index((uint32_t) index, 0, &header, sizeof(header), &bytes) || bytes != sizeof(header) ||
        header.magic != MYIMG_MAGIC || header.width == 0 || header.height == 0 || header.payload_size == 0) {
        return false;
    }
    uint32_t expected = header.format == MYIMG_RGB565 ? (uint32_t) header.width * header.height * 2 :
                        header.format == MYIMG_ARGB32 ? (uint32_t) header.width * header.height * 4 : 0;
    if (expected == 0 || expected != header.payload_size ||
        diskfs_index_size((uint32_t) index) < sizeof(header) + header.payload_size) {
        return false;
    }
    uint8_t *pixels = (uint8_t *) kmalloc(header.payload_size);
    if (pixels == 0) {
        return false;
    }
    if (!diskfs_read_index((uint32_t) index, sizeof(header), pixels, header.payload_size, &bytes) ||
        bytes != header.payload_size) {
        kfree(pixels);
        return false;
    }
    out->width = header.width;
    out->height = header.height;
    out->format = header.format;
    out->hotspot_x = header.hotspot_x;
    out->hotspot_y = header.hotspot_y;
    out->payload_size = header.payload_size;
    out->pixels = pixels;
    return true;
}

void assets_initialize(void)
{
    (void) load_myimg("/assets/wallpaper.myimg", &wallpaper);
    (void) load_myimg("/assets/cursor_pointer.myimg", &cursor);
}

static uint32_t rgb565_to_rgb888(uint16_t value)
{
    uint32_t r = (value >> 11) & 0x1F;
    uint32_t g = (value >> 5) & 0x3F;
    uint32_t b = value & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return (r << 16) | (g << 8) | b;
}

static uint32_t surface_color_to_rgb888(const struct graphics_surface *surface, uint32_t color)
{
    if (surface->bits_per_pixel == 16) {
        return rgb565_to_rgb888((uint16_t) color);
    }
    if (surface->bits_per_pixel == 24 || surface->bits_per_pixel == 32) {
        return color & 0x00FFFFFF;
    }
    return color;
}

static uint32_t blend_rgb(uint32_t dst, uint32_t src, uint32_t alpha)
{
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >> 8) & 0xFF;
    uint32_t sb = src & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >> 8) & 0xFF;
    uint32_t db = dst & 0xFF;
    uint32_t inv = 255 - alpha;
    return (((sr * alpha + dr * inv) / 255) << 16) |
           (((sg * alpha + dg * inv) / 255) << 8) |
           ((sb * alpha + db * inv) / 255);
}

bool assets_draw_wallpaper(struct graphics_surface *surface)
{
    if (!graphics_surface_is_valid(surface) || wallpaper.pixels == 0 || wallpaper.format != MYIMG_RGB565) {
        return false;
    }
    uint32_t scale_x = (surface->width + wallpaper.width - 1) / wallpaper.width;
    uint32_t scale_y = (surface->height + wallpaper.height - 1) / wallpaper.height;
    uint32_t scale = scale_x > scale_y ? scale_x : scale_y;
    uint32_t drawn_w = wallpaper.width * scale;
    uint32_t drawn_h = wallpaper.height * scale;
    uint32_t crop_x = drawn_w > surface->width ? (drawn_w - surface->width) / 2 : 0;
    uint32_t crop_y = drawn_h > surface->height ? (drawn_h - surface->height) / 2 : 0;

    const uint16_t *src = (const uint16_t *) wallpaper.pixels;
    for (uint32_t y = 0; y < surface->height; y++) {
        uint32_t sy = (y + crop_y) / scale;
        if (sy >= wallpaper.height) {
            sy = wallpaper.height - 1;
        }
        for (uint32_t x = 0; x < surface->width; x++) {
            uint32_t sx = (x + crop_x) / scale;
            if (sx >= wallpaper.width) {
                sx = wallpaper.width - 1;
            }
            graphics_put_pixel(surface, x, y, rgb565_to_rgb888(src[sy * wallpaper.width + sx]));
        }
    }
    return true;
}

bool assets_draw_cursor(struct graphics_surface *surface, int32_t x, int32_t y)
{
    if (!graphics_surface_is_valid(surface) || cursor.pixels == 0 || cursor.format != MYIMG_ARGB32) {
        return false;
    }
    const uint32_t *src = (const uint32_t *) cursor.pixels;
    int32_t left = x - (int32_t) cursor.hotspot_x;
    int32_t top = y - (int32_t) cursor.hotspot_y;
    for (uint32_t row = 0; row < cursor.height; row++) {
        for (uint32_t col = 0; col < cursor.width; col++) {
            int32_t px = left + (int32_t) col;
            int32_t py = top + (int32_t) row;
            if (px < 0 || py < 0 || px >= (int32_t) surface->width || py >= (int32_t) surface->height) {
                continue;
            }
            uint32_t argb = src[row * cursor.width + col];
            uint32_t alpha = argb >> 24;
            if (alpha == 0) {
                continue;
            }
            uint32_t color = argb & 0x00FFFFFF;
            if (alpha < 255) {
                uint32_t dst = surface_color_to_rgb888(surface, graphics_get_pixel(surface, (uint32_t) px, (uint32_t) py));
                color = blend_rgb(dst, color, alpha);
            }
            graphics_put_pixel(surface, (uint32_t) px, (uint32_t) py, color);
        }
    }
    return true;
}

uint32_t assets_cursor_width(void)
{
    return cursor.pixels != 0 ? cursor.width : 0;
}

uint32_t assets_cursor_height(void)
{
    return cursor.pixels != 0 ? cursor.height : 0;
}
