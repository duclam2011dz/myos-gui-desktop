#ifndef MYOS_GUI_NOTEPAD_HPP
#define MYOS_GUI_NOTEPAD_HPP

#include "../core/gui_types.hpp"

extern "C" {
#include "graphics.h"
}

namespace myos::gui {

class NotepadApp {
public:
    static constexpr uint32_t BUFFER_SIZE = 2048;

    void initialize(const Metrics &metrics);
    bool open_file(const char *path, const Metrics &metrics, uint32_t z);
    void close();
    void minimize();
    void toggle_maximize(const Metrics &metrics, uint32_t z);
    void focus(uint32_t z);
    void handle_key(char key);
    void draw(graphics_surface *surface) const;

    Window &window() { return window_; }
    const Window &window() const { return window_; }
    bool open() const { return window_.open; }
    bool minimized() const { return window_.mode == WindowMode::Minimized; }

private:
    Window window_ = {};
    char path_[32] = {};
    char buffer_[BUFFER_SIZE + 1] = {};
    uint32_t length_ = 0;
    uint32_t cursor_ = 0;
    bool dirty_ = false;
    bool last_save_ok_ = false;

    void default_geometry(const Metrics &metrics);
    void set_path(const char *path);
    void insert_char(char c);
    void backspace();
    void save();
    void draw_chrome(graphics_surface *surface) const;
    void draw_text(graphics_surface *surface) const;
};

} // namespace myos::gui

#endif
