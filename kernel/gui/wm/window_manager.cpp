#include "window_manager.hpp"

namespace myos::gui {

WindowArea WindowManager::hit_window(const Window &window, int32_t x, int32_t y) const
{
    if (!window.visible() || !window.rect().contains(x, y)) {
        return WindowArea::None;
    }

    if (Rect{window.x + window.w - 18, window.y + 4, 13, 11}.contains(x, y)) {
        return WindowArea::Close;
    }
    if (Rect{window.x + window.w - 33, window.y + 4, 13, 11}.contains(x, y)) {
        return WindowArea::Maximize;
    }
    if (Rect{window.x + window.w - 48, window.y + 4, 13, 11}.contains(x, y)) {
        return WindowArea::Minimize;
    }
    if (Rect{window.x + window.w - 10, window.y + window.h - 10, 10, 10}.contains(x, y) &&
        window.mode != WindowMode::Maximized) {
        return WindowArea::Resize;
    }
    if (Rect{window.x, window.y, window.w, TITLEBAR_H + 2}.contains(x, y) &&
        window.mode != WindowMode::Maximized) {
        return WindowArea::Titlebar;
    }
    return WindowArea::Client;
}

void WindowManager::begin_drag(Window &window, int32_t x, int32_t y)
{
    dragging_ = true;
    resizing_ = false;
    drag_offset_x_ = x - window.x;
    drag_offset_y_ = y - window.y;
}

void WindowManager::begin_resize()
{
    resizing_ = true;
    dragging_ = false;
}

void WindowManager::end_pointer_action()
{
    dragging_ = false;
    resizing_ = false;
}

Rect WindowManager::update_drag(Window &window, int32_t x, int32_t y, const Metrics &metrics)
{
    Rect before = window.rect();
    int32_t desktop_w = (int32_t) metrics.width;
    int32_t desktop_h = (int32_t) (metrics.height - metrics.taskbar_h);
    window.x = x - drag_offset_x_;
    window.y = y - drag_offset_y_;
    if (window.x < 0) {
        window.x = 0;
    }
    if (window.y < 0) {
        window.y = 0;
    }
    if (window.x + window.w > desktop_w) {
        window.x = desktop_w - window.w;
    }
    if (window.y + window.h > desktop_h) {
        window.y = desktop_h - window.h;
    }
    return before;
}

Rect WindowManager::update_resize(Window &window, int32_t x, int32_t y, const Metrics &metrics)
{
    Rect before = window.rect();
    int32_t desktop_w = (int32_t) metrics.width;
    int32_t desktop_h = (int32_t) (metrics.height - metrics.taskbar_h);
    window.w = x - window.x + 1;
    window.h = y - window.y + 1;
    if (window.w < WINDOW_MIN_W) {
        window.w = WINDOW_MIN_W;
    }
    if (window.h < WINDOW_MIN_H) {
        window.h = WINDOW_MIN_H;
    }
    if (window.x + window.w > desktop_w) {
        window.w = desktop_w - window.x;
    }
    if (window.y + window.h > desktop_h) {
        window.h = desktop_h - window.y;
    }
    return before;
}

} // namespace myos::gui

