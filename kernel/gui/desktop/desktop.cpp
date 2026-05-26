#include "desktop.hpp"

extern "C" {
#include "timer.h"
}

namespace myos::gui {

static void draw_wallpaper(graphics_surface *surface, const Metrics &metrics)
{
    for (uint32_t y = 0; y < metrics.height; y++) {
        uint32_t mix = metrics.height > 1 ? (y * 255) / (metrics.height - 1) : 0;
        uint32_t top = COLOR_DESKTOP_TOP;
        uint32_t bottom = COLOR_DESKTOP_BOTTOM;
        uint32_t r = (((top >> 16) & 0xFF) * (255 - mix) + ((bottom >> 16) & 0xFF) * mix) / 255;
        uint32_t g = (((top >> 8) & 0xFF) * (255 - mix) + ((bottom >> 8) & 0xFF) * mix) / 255;
        uint32_t b = ((top & 0xFF) * (255 - mix) + (bottom & 0xFF) * mix) / 255;
        graphics_fill_rect(surface, 0, y, metrics.width, 1, (r << 16) | (g << 8) | b);
    }

    uint32_t panel_w = metrics.width / 3;
    uint32_t panel_h = metrics.height / 4;
    if (panel_w > 90 && panel_h > 60) {
        graphics_draw_rect(surface, metrics.width - panel_w - 16, 18, panel_w, panel_h, 0x00436C7C);
        for (uint32_t i = 0; i < 5; i++) {
            graphics_fill_rect(surface, metrics.width - panel_w - 12 + i * 6, 22 + i * 5,
                               panel_w - 10 - i * 10, 1, 0x00284752);
        }
    }
}

static void draw_icon(graphics_surface *surface, int32_t x, int32_t y, const char *label, const char *glyph)
{
    uint32_t label_len = 0;
    while (label[label_len] != '\0') {
        label_len++;
    }
    int32_t label_w = (int32_t) label_len * 6 + 4;
    if (label_w < 46) {
        label_w = 46;
    }
    if (label_w > ICON_W - 2) {
        label_w = ICON_W - 2;
    }

    int32_t icon_x = x + (ICON_W - 30) / 2;
    int32_t label_x = x + (ICON_W - label_w) / 2;
    graphics_fill_rect(surface, icon_x, y, 30, 24, COLOR_WINDOW);
    graphics_draw_rect(surface, icon_x, y, 30, 24, 15);
    graphics_fill_rect(surface, icon_x + 3, y + 4, 24, 16, COLOR_TERMINAL_BG);
    graphics_draw_string(surface, icon_x + 8, y + 8, glyph, COLOR_TERMINAL_FG, COLOR_TERMINAL_BG);
    graphics_fill_rect(surface, label_x, y + 30, label_w, 11, COLOR_ACCENT_DARK);
    graphics_draw_string(surface, label_x + 2, y + 32, label, 15, COLOR_ACCENT_DARK);
}

Rect Desktop::start_menu_rect(const Metrics &metrics)
{
    int32_t menu_h = metrics.height >= 480 ? 114 : 72;
    int32_t menu_w = metrics.width >= 640 ? 184 : 126;
    return Rect{4, (int32_t) metrics.height - (int32_t) metrics.taskbar_h - menu_h - 2, menu_w, menu_h};
}

static void draw_start_menu(graphics_surface *surface, const Metrics &metrics, bool start_menu_open)
{
    if (!start_menu_open) {
        return;
    }

    Rect menu = Desktop::start_menu_rect(metrics);
    graphics_fill_rect(surface, menu.x, menu.y, menu.w, menu.h, COLOR_WINDOW);
    graphics_draw_rect(surface, menu.x, menu.y, menu.w, menu.h, COLOR_WINDOW_BORDER);
    graphics_fill_rect(surface, menu.x + 1, menu.y + 1, 22, menu.h - 2, COLOR_ACCENT_DARK);
    graphics_draw_string(surface, menu.x + 28, menu.y + 12, "TERMINAL", COLOR_TEXT_DARK, COLOR_WINDOW);
    graphics_draw_string(surface, menu.x + 28, menu.y + 28, "FILES", COLOR_TEXT_DARK, COLOR_WINDOW);
    graphics_draw_string(surface, menu.x + 28, menu.y + 44, "MONITOR", COLOR_TEXT_DARK, COLOR_WINDOW);
    graphics_draw_string(surface, menu.x + 28, menu.y + 60, "ABOUT", COLOR_TEXT_DARK, COLOR_WINDOW);
}

static void draw_task_button(graphics_surface *surface, const Metrics &metrics, int32_t x,
                             const char *label, bool minimized)
{
    uint32_t color = minimized ? 0x00434B54 : COLOR_ACCENT;
    graphics_fill_rect(surface, x, metrics.height - metrics.taskbar_h + 4, TASK_BUTTON_W, metrics.taskbar_h - 8, color);
    graphics_draw_rect(surface, x, metrics.height - metrics.taskbar_h + 4, TASK_BUTTON_W, metrics.taskbar_h - 8, 15);
    graphics_draw_string(surface, x + 8, metrics.height - metrics.taskbar_h + 7, label, 15, color);
}

static void draw_taskbar(graphics_surface *surface, const Metrics &metrics, bool start_menu_open,
                         bool terminal_open, bool terminal_minimized,
                         bool files_open, bool files_minimized,
                         bool monitor_open, bool monitor_minimized)
{
    graphics_fill_rect(surface, 0, metrics.height - metrics.taskbar_h, metrics.width, metrics.taskbar_h, COLOR_TASKBAR);
    graphics_fill_rect(surface, 0, metrics.height - metrics.taskbar_h, metrics.width, 1, COLOR_TASKBAR_HILITE);
    graphics_fill_rect(surface, 4, metrics.height - metrics.taskbar_h + 4, START_BUTTON_W, metrics.taskbar_h - 8,
                       start_menu_open ? COLOR_ACCENT : COLOR_ACCENT_DARK);
    graphics_draw_rect(surface, 4, metrics.height - metrics.taskbar_h + 4, START_BUTTON_W, metrics.taskbar_h - 8, 15);
    graphics_draw_string(surface, 11, metrics.height - metrics.taskbar_h + 7, "START", 15,
                         start_menu_open ? COLOR_ACCENT : COLOR_ACCENT_DARK);

    if (terminal_open) {
        draw_task_button(surface, metrics, TASK_BUTTON_X, "TERMINAL", terminal_minimized);
    }
    if (files_open) {
        draw_task_button(surface, metrics, TASK_BUTTON_X + TASK_BUTTON_W + 6, "FILES", files_minimized);
    }
    if (monitor_open) {
        draw_task_button(surface, metrics, TASK_BUTTON_X + (TASK_BUTTON_W + 6) * 2, "MONITOR", monitor_minimized);
    }

    uint32_t seconds = timer_uptime_seconds();
    uint32_t minutes = (seconds / 60) % 60;
    uint32_t hours = (seconds / 3600) % 24;
    char clock[18];
    clock[0] = (char) ('0' + (hours / 10));
    clock[1] = (char) ('0' + (hours % 10));
    clock[2] = ':';
    clock[3] = (char) ('0' + (minutes / 10));
    clock[4] = (char) ('0' + (minutes % 10));
    clock[5] = ' ';
    clock[6] = '2';
    clock[7] = '5';
    clock[8] = '/';
    clock[9] = '0';
    clock[10] = '5';
    clock[11] = '/';
    clock[12] = '2';
    clock[13] = '0';
    clock[14] = '2';
    clock[15] = '6';
    clock[16] = '\0';
    uint32_t clock_x = metrics.width > 108 ? metrics.width - 104 : 216;
    graphics_draw_string(surface, clock_x, metrics.height - metrics.taskbar_h + 7, clock, 15, COLOR_TASKBAR);
}

void Desktop::draw(graphics_surface *surface, const Metrics &metrics, bool start_menu_open,
                   bool terminal_open, bool terminal_minimized,
                   bool files_open, bool files_minimized,
                   bool monitor_open, bool monitor_minimized)
{
    draw_wallpaper(surface, metrics);
    draw_icon(surface, ICON_X, ICON_Y, "TERMINAL", ">_");
    draw_icon(surface, ICON_X, ICON_Y + 56, "FILE EXPLORER", "[]");
    draw_icon(surface, ICON_X, ICON_Y + 112, "SYSTEM MONITOR", "##");
    draw_icon(surface, ICON_X, ICON_Y + 168, "ABOUT", "i");
    draw_start_menu(surface, metrics, start_menu_open);
    draw_taskbar(surface, metrics, start_menu_open, terminal_open, terminal_minimized,
                 files_open, files_minimized, monitor_open, monitor_minimized);
}

} // namespace myos::gui
