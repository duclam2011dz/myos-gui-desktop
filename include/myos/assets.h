#ifndef MYOS_ASSETS_H
#define MYOS_ASSETS_H

#include <stdbool.h>
#include <stdint.h>

#include "graphics.h"

void assets_initialize(void);
bool assets_draw_wallpaper(struct graphics_surface *surface);
bool assets_draw_cursor(struct graphics_surface *surface, int32_t x, int32_t y, bool text_mode);
uint32_t assets_cursor_width(bool text_mode);
uint32_t assets_cursor_height(bool text_mode);

#endif
