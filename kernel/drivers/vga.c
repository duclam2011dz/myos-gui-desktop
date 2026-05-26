#include "vga.h"

#include "graphics.h"

static const size_t VGA_WIDTH = 53;
static const size_t VGA_HEIGHT = 24;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static char terminal_chars[24][53];
static uint8_t terminal_colors[24][53];

static uint32_t color_to_graphics(uint8_t color)
{
    switch (color & 0x0F) {
    case VGA_COLOR_BLUE: return 1;
    case VGA_COLOR_GREEN: return 2;
    case VGA_COLOR_CYAN: return 3;
    case VGA_COLOR_RED: return 4;
    case VGA_COLOR_MAGENTA: return 5;
    case VGA_COLOR_BROWN: return 6;
    case VGA_COLOR_LIGHT_GREY: return 7;
    case VGA_COLOR_DARK_GREY: return 8;
    case VGA_COLOR_LIGHT_BLUE: return 9;
    case VGA_COLOR_LIGHT_GREEN: return 10;
    case VGA_COLOR_LIGHT_CYAN: return 11;
    case VGA_COLOR_LIGHT_RED: return 12;
    case VGA_COLOR_LIGHT_MAGENTA: return 13;
    case VGA_COLOR_LIGHT_BROWN: return 14;
    case VGA_COLOR_WHITE: return 15;
    default: return 0;
    }
}

static void terminal_draw_cell(size_t x, size_t y)
{
    struct graphics_surface *surface = graphics_primary_surface_mut();
    if (surface == 0) {
        return;
    }

    graphics_draw_char(surface, (uint32_t) x * 6, (uint32_t) y * 8,
                       terminal_chars[y][x], color_to_graphics(terminal_colors[y][x]), 0);
}

static void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return;
    }

    terminal_chars[y][x] = c;
    terminal_colors[y][x] = color;
    terminal_draw_cell(x, y);
}

static void terminal_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_chars[y - 1][x] = terminal_chars[y][x];
            terminal_colors[y - 1][x] = terminal_colors[y][x];
            terminal_draw_cell(x, y - 1);
        }
    }

    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_putentryat(' ', terminal_color, x, VGA_HEIGHT - 1);
    }

    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c)
{
    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
        return;
    }

    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);

    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_scroll();
        }
    }
}

void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_clear();
}

void terminal_clear(void)
{
    struct graphics_surface *surface = graphics_primary_surface_mut();
    if (surface != 0) {
        graphics_clear(surface, 0);
    }

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_chars[y][x] = ' ';
            terminal_colors[y][x] = terminal_color;
        }
    }

    terminal_row = 0;
    terminal_column = 0;
}

void terminal_set_color(uint8_t color)
{
    terminal_color = color;
}

void terminal_write(const char *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        terminal_putchar(data[i]);
    }
}

void terminal_writestring(const char *data)
{
    size_t size = 0;
    while (data[size] != '\0') {
        size++;
    }

    terminal_write(data, size);
}
