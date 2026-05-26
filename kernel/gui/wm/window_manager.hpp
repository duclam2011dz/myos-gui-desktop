#ifndef MYOS_GUI_WINDOW_MANAGER_HPP
#define MYOS_GUI_WINDOW_MANAGER_HPP

#include "../core/gui_types.hpp"

namespace myos::gui {

enum class WindowArea : uint8_t {
    None,
    Client,
    Titlebar,
    Minimize,
    Maximize,
    Close,
    Resize,
};

class WindowManager {
public:
    WindowArea hit_window(const Window &window, int32_t x, int32_t y) const;
    void begin_drag(Window &window, int32_t x, int32_t y);
    void begin_resize();
    void end_pointer_action();
    bool dragging() const { return dragging_; }
    bool resizing() const { return resizing_; }
    Rect update_drag(Window &window, int32_t x, int32_t y, const Metrics &metrics);
    Rect update_resize(Window &window, int32_t x, int32_t y, const Metrics &metrics);

private:
    bool dragging_ = false;
    bool resizing_ = false;
    int32_t drag_offset_x_ = 0;
    int32_t drag_offset_y_ = 0;
};

} // namespace myos::gui

#endif
