#include "utility_apps.hpp"

extern "C" {
#include "diskfs.h"
#include "heap.h"
#include "pmm.h"
#include "scheduler.h"
#include "timer.h"
#include "usermode.h"
#include "util.h"
}

namespace myos::gui {

const char *UtilityApp::title() const
{
    if (kind_ == UtilityKind::Files) {
        return "FILE EXPLORER";
    }
    if (kind_ == UtilityKind::Monitor) {
        return "SYSTEM MONITOR";
    }
    return "ABOUT MYOS";
}

void UtilityApp::default_geometry(const Metrics &metrics)
{
    window_.w = metrics.width >= 640 ? 360 : 230;
    window_.h = metrics.height >= 480 ? 260 : 150;
    if (window_.w > (int32_t) metrics.width - 12) {
        window_.w = (int32_t) metrics.width - 12;
    }
    if (window_.h > (int32_t) metrics.height - (int32_t) metrics.taskbar_h - 12) {
        window_.h = (int32_t) metrics.height - (int32_t) metrics.taskbar_h - 12;
    }
    if (window_.w < WINDOW_MIN_W) {
        window_.w = WINDOW_MIN_W;
    }
    if (window_.h < WINDOW_MIN_H) {
        window_.h = WINDOW_MIN_H;
    }

    int32_t offset = kind_ == UtilityKind::Files ? -36 : (kind_ == UtilityKind::Monitor ? 16 : 56);
    window_.x = (int32_t) ((metrics.width - (uint32_t) window_.w) / 2) + offset;
    window_.y = metrics.height > 260 ? 58 + (offset / 4) : 30;
    if (window_.x < 4) {
        window_.x = 4;
    }
    if (window_.y < 4) {
        window_.y = 4;
    }
}

void UtilityApp::initialize(UtilityKind kind, const Metrics &metrics)
{
    kind_ = kind;
    default_geometry(metrics);
    window_.restore_x = window_.x;
    window_.restore_y = window_.y;
    window_.restore_w = window_.w;
    window_.restore_h = window_.h;
    window_.open = false;
    window_.focused = false;
    window_.mode = WindowMode::Normal;
    window_.z = 0;
}

void UtilityApp::open_window(const Metrics &metrics, uint32_t z)
{
    if (!window_.open) {
        window_.open = true;
        window_.mode = WindowMode::Normal;
        default_geometry(metrics);
        window_.restore_x = window_.x;
        window_.restore_y = window_.y;
        window_.restore_w = window_.w;
        window_.restore_h = window_.h;
    }
    focus(z);
}

void UtilityApp::close()
{
    window_.open = false;
    window_.focused = false;
    window_.mode = WindowMode::Normal;
}

void UtilityApp::minimize()
{
    window_.mode = WindowMode::Minimized;
    window_.focused = false;
}

void UtilityApp::toggle_maximize(const Metrics &metrics, uint32_t z)
{
    if (window_.mode == WindowMode::Maximized) {
        window_.mode = WindowMode::Normal;
        window_.x = window_.restore_x;
        window_.y = window_.restore_y;
        window_.w = window_.restore_w;
        window_.h = window_.restore_h;
    } else {
        window_.restore_x = window_.x;
        window_.restore_y = window_.y;
        window_.restore_w = window_.w;
        window_.restore_h = window_.h;
        window_.mode = WindowMode::Maximized;
        window_.x = 2;
        window_.y = 4;
        window_.w = metrics.width - 4;
        window_.h = metrics.height - metrics.taskbar_h - 6;
    }
    focus(z);
}

void UtilityApp::focus(uint32_t z)
{
    if (!window_.open) {
        return;
    }
    window_.focused = true;
    window_.z = z;
}

void UtilityApp::draw_chrome(graphics_surface *surface) const
{
    uint32_t title_color = window_.focused ? COLOR_ACCENT : 0x00505E6A;
    graphics_fill_rect(surface, window_.x, window_.y, window_.w, window_.h, COLOR_WINDOW);
    graphics_draw_rect(surface, window_.x, window_.y, window_.w, window_.h,
                       window_.focused ? COLOR_ACCENT : COLOR_WINDOW_BORDER);
    graphics_fill_rect(surface, window_.x + 1, window_.y + 1, window_.w - 2, TITLEBAR_H, title_color);
    graphics_draw_string(surface, window_.x + 7, window_.y + 6, title(), 15, title_color);
    graphics_fill_rect(surface, window_.x + window_.w - 48, window_.y + 4, 13, 11, 0x006D7781);
    graphics_draw_string(surface, window_.x + window_.w - 44, window_.y + 6, "_", 15, 0x006D7781);
    graphics_fill_rect(surface, window_.x + window_.w - 33, window_.y + 4, 13, 11, 0x006D7781);
    graphics_draw_string(surface, window_.x + window_.w - 30, window_.y + 6, "M", 15, 0x006D7781);
    graphics_fill_rect(surface, window_.x + window_.w - 18, window_.y + 4, 13, 11, 0x00B84A4A);
    graphics_draw_string(surface, window_.x + window_.w - 14, window_.y + 6, "X", 15, 0x00B84A4A);
    graphics_fill_rect(surface, window_.x + 6, window_.y + TITLEBAR_H + 5,
                       window_.w - 12, window_.h - TITLEBAR_H - 12, 0x00FFFFFF);
}

static void draw_u32(graphics_surface *surface, int32_t x, int32_t y, const char *label, uint32_t value)
{
    char text[16];
    u32_to_dec(value, text, sizeof(text));
    graphics_draw_string(surface, x, y, label, COLOR_TEXT_DARK, 0x00FFFFFF);
    graphics_draw_string(surface, x + 108, y, text, COLOR_ACCENT_DARK, 0x00FFFFFF);
}

static bool has_suffix(const char *name, const char *suffix)
{
    uint32_t name_len = 0;
    uint32_t suffix_len = 0;
    while (name[name_len] != '\0') {
        name_len++;
    }
    while (suffix[suffix_len] != '\0') {
        suffix_len++;
    }
    if (suffix_len > name_len) {
        return false;
    }
    uint32_t start = name_len - suffix_len;
    for (uint32_t i = 0; i < suffix_len; i++) {
        if (name[start + i] != suffix[i]) {
            return false;
        }
    }
    return true;
}

static const char *file_icon_label(const char *name)
{
    if (has_suffix(name, ".elf")) {
        return "ELF";
    }
    if (has_suffix(name, ".mx")) {
        return "MX";
    }
    if (has_suffix(name, ".myimg")) {
        return "MYIMG";
    }
    if (has_suffix(name, ".txt") || has_suffix(name, "/motd")) {
        return "TXT";
    }
    return "FILE";
}

void UtilityApp::draw_files(graphics_surface *surface) const
{
    int32_t x = window_.x + 14;
    int32_t y = window_.y + TITLEBAR_H + 14;
    graphics_draw_string(surface, x, y, "DISKFS FILES", COLOR_ACCENT_DARK, 0x00FFFFFF);
    y += 14;
    size_t count = diskfs_file_count();
    for (size_t i = 0; i < count && y < window_.y + window_.h - 12; i++) {
        const char *name = diskfs_file_name(i);
        graphics_fill_rect(surface, x - 4, y - 2, window_.w - 28, 11, (i % 2) == 0 ? 0x00EEF3F8 : 0x00FFFFFF);
        graphics_draw_string(surface, x, y, file_icon_label(name), COLOR_ACCENT_DARK, (i % 2) == 0 ? 0x00EEF3F8 : 0x00FFFFFF);
        graphics_draw_string(surface, x + 38, y, name, COLOR_TEXT_DARK, (i % 2) == 0 ? 0x00EEF3F8 : 0x00FFFFFF);
        y += 12;
    }
}

int UtilityApp::file_index_at(int32_t x, int32_t y) const
{
    if (kind_ != UtilityKind::Files || !window_.visible()) {
        return -1;
    }
    int32_t row_x = window_.x + 10;
    int32_t row_y = window_.y + TITLEBAR_H + 26;
    int32_t row_w = window_.w - 28;
    size_t count = diskfs_file_count();
    for (size_t i = 0; i < count; i++) {
        Rect row = {row_x, row_y + (int32_t) i * 12, row_w, 11};
        if (row.contains(x, y)) {
            return (int) i;
        }
    }
    return -1;
}

void UtilityApp::draw_monitor(graphics_surface *surface) const
{
    int32_t x = window_.x + 14;
    int32_t y = window_.y + TITLEBAR_H + 16;
    char fsck_report[48];
    draw_u32(surface, x, y, "UPTIME", timer_uptime_seconds());
    draw_u32(surface, x, y + 14, "TICKS", timer_ticks());
    draw_u32(surface, x, y + 28, "FREE PAGES", pmm_free_pages());
    draw_u32(surface, x, y + 42, "HEAP FREE", (uint32_t) heap_free_bytes());
    draw_u32(surface, x, y + 56, "SWITCHES", scheduler_switch_count());
    draw_u32(surface, x, y + 70, "PROCESSES", user_process_count());
    fsck_report[0] = '\0';
    draw_u32(surface, x, y + 84, "FSCK ERR", diskfs_fsck(fsck_report, sizeof(fsck_report)));
}

void UtilityApp::draw_about(graphics_surface *surface) const
{
    int32_t x = window_.x + 14;
    int32_t y = window_.y + TITLEBAR_H + 16;
    graphics_draw_string(surface, x, y, "MYOS DESKTOP", COLOR_ACCENT_DARK, 0x00FFFFFF);
    graphics_draw_string(surface, x, y + 16, "FREESTANDING C++ GUI", COLOR_TEXT_DARK, 0x00FFFFFF);
    graphics_draw_string(surface, x, y + 28, "WINDOW MANAGER + APPS", COLOR_TEXT_DARK, 0x00FFFFFF);
    graphics_draw_string(surface, x, y + 40, "DISKFS, MONITOR, TERMINAL", COLOR_TEXT_DARK, 0x00FFFFFF);
}

void UtilityApp::draw(graphics_surface *surface) const
{
    if (!window_.visible()) {
        return;
    }
    draw_chrome(surface);
    if (kind_ == UtilityKind::Files) {
        draw_files(surface);
    } else if (kind_ == UtilityKind::Monitor) {
        draw_monitor(surface);
    } else {
        draw_about(surface);
    }
}

} // namespace myos::gui
