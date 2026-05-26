#ifndef MYOS_GUI_TYPES_HPP
#define MYOS_GUI_TYPES_HPP

#include <stdbool.h>
#include <stdint.h>

namespace myos::gui {

static constexpr uint32_t FALLBACK_W = 320;
static constexpr uint32_t FALLBACK_H = 200;
static constexpr int32_t ICON_X = 10;
static constexpr int32_t ICON_Y = 30;
static constexpr int32_t ICON_W = 96;
static constexpr int32_t ICON_H = 46;
static constexpr int32_t TITLEBAR_H = 18;
static constexpr int32_t WINDOW_MIN_W = 168;
static constexpr int32_t WINDOW_MIN_H = 82;
static constexpr int32_t START_BUTTON_W = 58;
static constexpr int32_t TASK_BUTTON_X = 70;
static constexpr int32_t TASK_BUTTON_W = 92;
static constexpr int32_t CLICK_SLOP = 6;
static constexpr uint32_t DOUBLE_CLICK_TICKS = 80;

static constexpr uint32_t COLOR_DESKTOP_TOP = 0x00306078;
static constexpr uint32_t COLOR_DESKTOP_BOTTOM = 0x00163440;
static constexpr uint32_t COLOR_TASKBAR = 0x00202A33;
static constexpr uint32_t COLOR_TASKBAR_HILITE = 0x00344550;
static constexpr uint32_t COLOR_ACCENT = 0x000078D4;
static constexpr uint32_t COLOR_ACCENT_DARK = 0x00005296;
static constexpr uint32_t COLOR_WINDOW = 0x00EEF3F8;
static constexpr uint32_t COLOR_WINDOW_BORDER = 0x004A5D6E;
static constexpr uint32_t COLOR_TERMINAL_BG = 0x00071116;
static constexpr uint32_t COLOR_TERMINAL_FG = 0x0058D77A;
static constexpr uint32_t COLOR_TEXT_DARK = 0x0016212B;

enum class WindowMode : uint8_t {
    Normal,
    Minimized,
    Maximized,
};

enum class HitTarget : uint8_t {
    None,
    Desktop,
    DesktopIcon,
    StartButton,
    StartTerminal,
    TaskButton,
    WindowClient,
    WindowTitlebar,
    WindowMinimize,
    WindowMaximize,
    WindowClose,
    WindowResize,
};

enum class AppId : uint8_t {
    None,
    Terminal,
    Files,
    Monitor,
    About,
    Notepad,
};

struct HitResult {
    HitTarget target;
    AppId app;
};

struct Rect {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;

    bool empty() const { return w <= 0 || h <= 0; }
    bool contains(int32_t px, int32_t py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

struct Metrics {
    uint32_t width;
    uint32_t height;
    uint32_t taskbar_h;
};

struct Window {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t restore_x;
    int32_t restore_y;
    int32_t restore_w;
    int32_t restore_h;
    bool open;
    bool focused;
    WindowMode mode;
    uint32_t z;

    Rect rect() const { return Rect{x, y, w, h}; }
    bool visible() const { return open && mode != WindowMode::Minimized; }
};

inline int32_t min_i32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

inline int32_t max_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

} // namespace myos::gui

#endif
