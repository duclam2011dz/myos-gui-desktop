#include "desktop.hpp"

extern "C" {
#include "assets.h"
#include "rtc.h"
#include "timer.h"
}

namespace myos::gui {

static void draw_wallpaper(graphics_surface *surface, const Metrics &metrics)
{
    if (!assets_draw_wallpaper(surface)) {
        graphics_fill_rect(surface, 0, 0, metrics.width, metrics.height, COLOR_DESKTOP_BOTTOM);
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
    int32_t menu_h = metrics.height >= 480 ? 150 : 108;
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
    graphics_fill_rect(surface, menu.x + 26, menu.y + 78, menu.w - 32, 1, COLOR_WINDOW_BORDER);
    graphics_draw_string(surface, menu.x + 28, menu.y + 88, "SHUTDOWN", COLOR_TEXT_DARK, COLOR_WINDOW);
    graphics_draw_string(surface, menu.x + 28, menu.y + 104, "RESTART", COLOR_TEXT_DARK, COLOR_WINDOW);
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

    struct rtc_datetime now;
    bool has_rtc = rtc_read_datetime(&now);
    uint32_t seconds = timer_uptime_seconds();
    uint32_t minutes = has_rtc ? now.minute : (seconds / 60) % 60;
    uint32_t hours = has_rtc ? now.hour : (seconds / 3600) % 24;
    uint32_t day = has_rtc ? now.day : 25;
    uint32_t month = has_rtc ? now.month : 5;
    uint32_t year = has_rtc ? now.year : 2026;
    char clock[18];
    clock[0] = (char) ('0' + (hours / 10));
    clock[1] = (char) ('0' + (hours % 10));
    clock[2] = ':';
    clock[3] = (char) ('0' + (minutes / 10));
    clock[4] = (char) ('0' + (minutes % 10));
    clock[5] = ' ';
    clock[6] = (char) ('0' + (day / 10));
    clock[7] = (char) ('0' + (day % 10));
    clock[8] = '/';
    clock[9] = (char) ('0' + (month / 10));
    clock[10] = (char) ('0' + (month % 10));
    clock[11] = '/';
    clock[12] = (char) ('0' + ((year / 1000) % 10));
    clock[13] = (char) ('0' + ((year / 100) % 10));
    clock[14] = (char) ('0' + ((year / 10) % 10));
    clock[15] = (char) ('0' + (year % 10));
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
