#include "notepad.hpp"

extern "C" {
#include "diskfs.h"
#include "gui_event.h"
#include "serial.h"
}

namespace myos::gui {

void NotepadApp::default_geometry(const Metrics &metrics)
{
    window_.w = metrics.width >= 640 ? 430 : 248;
    window_.h = metrics.height >= 480 ? 300 : 160;
    if (window_.w > (int32_t) metrics.width - 10) {
        window_.w = (int32_t) metrics.width - 10;
    }
    if (window_.h > (int32_t) metrics.height - (int32_t) metrics.taskbar_h - 10) {
        window_.h = (int32_t) metrics.height - (int32_t) metrics.taskbar_h - 10;
    }
    if (window_.w < WINDOW_MIN_W) {
        window_.w = WINDOW_MIN_W;
    }
    if (window_.h < WINDOW_MIN_H) {
        window_.h = WINDOW_MIN_H;
    }
    window_.x = (int32_t) ((metrics.width - (uint32_t) window_.w) / 2) + 28;
    window_.y = metrics.height > 260 ? 72 : 24;
}

void NotepadApp::initialize(const Metrics &metrics)
{
    default_geometry(metrics);
    window_.restore_x = window_.x;
    window_.restore_y = window_.y;
    window_.restore_w = window_.w;
    window_.restore_h = window_.h;
    window_.open = false;
    window_.focused = false;
    window_.mode = WindowMode::Normal;
    window_.z = 0;
    path_[0] = '\0';
    buffer_[0] = '\0';
    length_ = 0;
    cursor_ = 0;
    dirty_ = false;
    last_save_ok_ = false;
}

void NotepadApp::set_path(const char *path)
{
    uint32_t i = 0;
    for (; i + 1 < sizeof(path_) && path[i] != '\0'; i++) {
        path_[i] = path[i];
    }
    path_[i] = '\0';
}

bool NotepadApp::open_file(const char *path, const Metrics &metrics, uint32_t z)
{
    size_t size = 0;
    const char *data = diskfs_read_file(path, &size);
    if (data == nullptr || size > BUFFER_SIZE) {
        return false;
    }

    set_path(path);
    for (uint32_t i = 0; i < size; i++) {
        buffer_[i] = data[i];
    }
    buffer_[size] = '\0';
    length_ = (uint32_t) size;
    cursor_ = length_;
    dirty_ = false;
    last_save_ok_ = false;
    if (!window_.open) {
        default_geometry(metrics);
        window_.restore_x = window_.x;
        window_.restore_y = window_.y;
        window_.restore_w = window_.w;
        window_.restore_h = window_.h;
    }
    window_.open = true;
    window_.mode = WindowMode::Normal;
    focus(z);
    serial_writestring("MyOS GUI: Notepad opened file.\n");
    return true;
}

void NotepadApp::close()
{
    window_.open = false;
    window_.focused = false;
    window_.mode = WindowMode::Normal;
}

void NotepadApp::minimize()
{
    window_.mode = WindowMode::Minimized;
    window_.focused = false;
}

void NotepadApp::toggle_maximize(const Metrics &metrics, uint32_t z)
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

void NotepadApp::focus(uint32_t z)
{
    if (!window_.open) {
        return;
    }
    window_.focused = true;
    window_.z = z;
}

void NotepadApp::insert_char(char c)
{
    if (length_ >= BUFFER_SIZE) {
        return;
    }
    for (uint32_t i = length_; i > cursor_; i--) {
        buffer_[i] = buffer_[i - 1];
    }
    buffer_[cursor_++] = c;
    length_++;
    buffer_[length_] = '\0';
    dirty_ = true;
}

void NotepadApp::backspace()
{
    if (cursor_ == 0) {
        return;
    }
    for (uint32_t i = cursor_ - 1; i < length_; i++) {
        buffer_[i] = buffer_[i + 1];
    }
    cursor_--;
    length_--;
    dirty_ = true;
}

void NotepadApp::save()
{
    last_save_ok_ = diskfs_write_file(path_, buffer_, length_);
    if (last_save_ok_) {
        dirty_ = false;
        serial_writestring("MyOS GUI: Notepad saved file.\n");
    } else {
        serial_writestring("MyOS GUI: Notepad save failed.\n");
    }
}

void NotepadApp::handle_key(char key)
{
    if (!window_.open || window_.mode == WindowMode::Minimized || !window_.focused) {
        return;
    }
    if (key == GUI_KEY_CTRL_S) {
        save();
        return;
    }
    if (key == '\b') {
        backspace();
        return;
    }
    if (key == '\n') {
        insert_char('\n');
        return;
    }
    if (key >= ' ' && key <= '~') {
        insert_char(key);
    }
}

void NotepadApp::draw_chrome(graphics_surface *surface) const
{
    uint32_t title_color = window_.focused ? COLOR_ACCENT : 0x00505E6A;
    graphics_fill_rect(surface, window_.x, window_.y, window_.w, window_.h, COLOR_WINDOW);
    graphics_draw_rect(surface, window_.x, window_.y, window_.w, window_.h,
                       window_.focused ? COLOR_ACCENT : COLOR_WINDOW_BORDER);
    graphics_fill_rect(surface, window_.x + 1, window_.y + 1, window_.w - 2, TITLEBAR_H, title_color);
    graphics_draw_string(surface, window_.x + 7, window_.y + 6, "NOTEPAD", 15, title_color);
    graphics_fill_rect(surface, window_.x + window_.w - 48, window_.y + 4, 13, 11, 0x006D7781);
    graphics_draw_string(surface, window_.x + window_.w - 44, window_.y + 6, "_", 15, 0x006D7781);
    graphics_fill_rect(surface, window_.x + window_.w - 33, window_.y + 4, 13, 11, 0x006D7781);
    graphics_draw_string(surface, window_.x + window_.w - 30, window_.y + 6, "M", 15, 0x006D7781);
    graphics_fill_rect(surface, window_.x + window_.w - 18, window_.y + 4, 13, 11, 0x00B84A4A);
    graphics_draw_string(surface, window_.x + window_.w - 14, window_.y + 6, "X", 15, 0x00B84A4A);
    graphics_fill_rect(surface, window_.x + 6, window_.y + TITLEBAR_H + 5,
                       window_.w - 12, window_.h - TITLEBAR_H - 22, 0x00FFFFFF);
    graphics_fill_rect(surface, window_.x + 6, window_.y + window_.h - 15, window_.w - 12, 10, 0x00EEF3F8);
}

void NotepadApp::draw_text(graphics_surface *surface) const
{
    int32_t x = window_.x + 10;
    int32_t y = window_.y + TITLEBAR_H + 10;
    int32_t base_x = x;
    int32_t max_y = window_.y + window_.h - 20;
    for (uint32_t i = 0; i < length_ && y < max_y; i++) {
        if (buffer_[i] == '\n') {
            x = base_x;
            y += 8;
            continue;
        }
        graphics_draw_char(surface, x, y, buffer_[i], COLOR_TEXT_DARK, 0x00FFFFFF);
        x += 6;
        if (x + 6 > window_.x + window_.w - 12) {
            x = base_x;
            y += 8;
        }
    }

    const char *status = dirty_ ? "MODIFIED  CTRL+S SAVE" : (last_save_ok_ ? "SAVED" : "CTRL+S SAVE");
    graphics_draw_string(surface, window_.x + 10, window_.y + window_.h - 13, path_, COLOR_ACCENT_DARK, 0x00EEF3F8);
    graphics_draw_string(surface, window_.x + window_.w - 126, window_.y + window_.h - 13, status, COLOR_TEXT_DARK, 0x00EEF3F8);
}

void NotepadApp::draw(graphics_surface *surface) const
{
    if (!window_.visible()) {
        return;
    }
    draw_chrome(surface);
    draw_text(surface);
}

} // namespace myos::gui
