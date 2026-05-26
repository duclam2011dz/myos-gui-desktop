#ifndef MYOS_GUI_UTILITY_APPS_HPP
#define MYOS_GUI_UTILITY_APPS_HPP

#include "../core/gui_types.hpp"

extern "C" {
#include "graphics.h"
}

namespace myos::gui {

enum class UtilityKind : uint8_t {
    Files,
    Monitor,
    About,
};

class UtilityApp {
public:
    void initialize(UtilityKind kind, const Metrics &metrics);
    void open_window(const Metrics &metrics, uint32_t z);
    void close();
    void minimize();
    void toggle_maximize(const Metrics &metrics, uint32_t z);
    void focus(uint32_t z);
    void draw(graphics_surface *surface) const;
    int file_index_at(int32_t x, int32_t y) const;

    Window &window() { return window_; }
    const Window &window() const { return window_; }
    bool open() const { return window_.open; }
    bool minimized() const { return window_.mode == WindowMode::Minimized; }

private:
    UtilityKind kind_ = UtilityKind::Files;
    Window window_ = {};

    const char *title() const;
    void default_geometry(const Metrics &metrics);
    void draw_chrome(graphics_surface *surface) const;
    void draw_files(graphics_surface *surface) const;
    void draw_monitor(graphics_surface *surface) const;
    void draw_about(graphics_surface *surface) const;
};

} // namespace myos::gui

#endif
