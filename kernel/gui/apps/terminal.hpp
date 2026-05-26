#ifndef MYOS_GUI_TERMINAL_HPP
#define MYOS_GUI_TERMINAL_HPP

#include "../core/gui_types.hpp"

extern "C" {
#include "graphics.h"
}

namespace myos::gui {

class TerminalApp {
public:
    static constexpr uint32_t COLS = 160;
    static constexpr uint32_t HISTORY_ROWS = 96;
    static constexpr uint32_t COMMAND_SIZE = 96;
    static constexpr uint32_t COMMAND_HISTORY = 8;

    void initialize(const Metrics &metrics);
    void write(const char *text);
    void draw(graphics_surface *surface) const;
    void open_window(const Metrics &metrics, uint32_t z);
    void minimize();
    void toggle_maximize(const Metrics &metrics, uint32_t z);
    void close();
    void focus(uint32_t z);
    void handle_key(char key, const Metrics &metrics, uint32_t z);
    bool hit(int32_t x, int32_t y) const;

    Window &window() { return window_; }
    const Window &window() const { return window_; }
    bool open() const { return window_.open; }
    bool minimized() const { return window_.mode == WindowMode::Minimized; }

private:
    Window window_;
    char lines_[HISTORY_ROWS][COLS + 1];
    uint32_t line_count_;
    uint32_t col_;
    uint32_t scroll_;
    char command_line_[COMMAND_SIZE];
    uint32_t command_len_;
    char command_history_[COMMAND_HISTORY][COMMAND_SIZE];
    uint32_t command_history_count_;
    int32_t command_history_index_;

    uint32_t text_cols() const;
    uint32_t visible_rows() const;
    void clear_buffer();
    void newline();
    void default_geometry(const Metrics &metrics);
    void print(const char *text);
    void print_u32(const char *prefix, uint32_t value);
    void push_history(const char *line);
    void recall_history(int32_t delta);
    void execute_command(const char *line);
};

} // namespace myos::gui

#endif
