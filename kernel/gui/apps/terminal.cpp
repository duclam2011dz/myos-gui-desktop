#include "terminal.hpp"

extern "C" {
#include "diskfs.h"
#include "gui_event.h"
#include "heap.h"
#include "pci.h"
#include "pmm.h"
#include "scheduler.h"
#include "serial.h"
#include "timer.h"
#include "usermode.h"
#include "util.h"
}

namespace myos::gui {

uint32_t TerminalApp::text_cols() const
{
    if (window_.w <= 18) {
        return 1;
    }

    uint32_t cols = (uint32_t) (window_.w - 16) / 6;
    if (cols > COLS) {
        cols = COLS;
    }
    return cols == 0 ? 1 : cols;
}

uint32_t TerminalApp::visible_rows() const
{
    if (window_.h < TITLEBAR_H + 18) {
        return 1;
    }
    uint32_t rows = (uint32_t) (window_.h - TITLEBAR_H - 12) / 8;
    return rows == 0 ? 1 : rows;
}

void TerminalApp::clear_buffer()
{
    for (uint32_t y = 0; y < HISTORY_ROWS; y++) {
        for (uint32_t x = 0; x < COLS; x++) {
            lines_[y][x] = ' ';
        }
        lines_[y][COLS] = '\0';
    }
    line_count_ = 1;
    col_ = 0;
    scroll_ = 0;
}

void TerminalApp::newline()
{
    col_ = 0;
    if (line_count_ < HISTORY_ROWS) {
        line_count_++;
        for (uint32_t x = 0; x < COLS; x++) {
            lines_[line_count_ - 1][x] = ' ';
        }
    } else {
        for (uint32_t y = 1; y < HISTORY_ROWS; y++) {
            for (uint32_t x = 0; x < COLS; x++) {
                lines_[y - 1][x] = lines_[y][x];
            }
        }
        for (uint32_t x = 0; x < COLS; x++) {
            lines_[HISTORY_ROWS - 1][x] = ' ';
        }
    }
}

void TerminalApp::write(const char *text)
{
    if (line_count_ == 0) {
        line_count_ = 1;
    }

    for (uint32_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        uint32_t row = line_count_ - 1;
        if (c == '\n') {
            newline();
            continue;
        }
        if (c == '\b') {
            if (col_ > 0) {
                col_--;
                lines_[row][col_] = ' ';
            }
            continue;
        }
        if (c < ' ' || c > '~') {
            continue;
        }
        uint32_t cols = text_cols();
        if (col_ >= cols) {
            newline();
            row = line_count_ - 1;
        }
        lines_[row][col_] = c;
        if (++col_ >= cols) {
            newline();
        }
    }
    scroll_ = 0;
}

void TerminalApp::print(const char *text)
{
    write(text);
    serial_writestring(text);
}

void TerminalApp::print_u32(const char *prefix, uint32_t value)
{
    char number[12];
    print(prefix);
    u32_to_dec(value, number, sizeof(number));
    print(number);
    print("\n");
}

void TerminalApp::push_history(const char *line)
{
    if (line[0] == '\0') {
        return;
    }
    uint32_t slot = command_history_count_ % COMMAND_HISTORY;
    uint32_t i = 0;
    for (; i + 1 < COMMAND_SIZE && line[i] != '\0'; i++) {
        command_history_[slot][i] = line[i];
    }
    command_history_[slot][i] = '\0';
    command_history_count_++;
    command_history_index_ = -1;
}

void TerminalApp::default_geometry(const Metrics &metrics)
{
    window_.w = metrics.width > 96 ? (int32_t) (metrics.width - 96) : 244;
    window_.h = metrics.height > 140 ? (int32_t) (metrics.height - metrics.taskbar_h - 74) : 134;
    if (window_.w > 640) {
        window_.w = 640;
    }
    if (window_.h > 360) {
        window_.h = 360;
    }
    if (window_.w < WINDOW_MIN_W) {
        window_.w = WINDOW_MIN_W;
    }
    if (window_.h < 134) {
        window_.h = 134;
    }
    if ((uint32_t) window_.w > metrics.width - 8) {
        window_.w = (int32_t) metrics.width - 8;
    }
    if ((uint32_t) window_.h > metrics.height - metrics.taskbar_h - 8) {
        window_.h = (int32_t) (metrics.height - metrics.taskbar_h - 8);
    }
    window_.x = (int32_t) ((metrics.width - (uint32_t) window_.w) / 2);
    window_.y = metrics.height > 260 ? 42 : 26;
}

void TerminalApp::initialize(const Metrics &metrics)
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
    command_len_ = 0;
    command_history_count_ = 0;
    command_history_index_ = -1;
    clear_buffer();
}

void TerminalApp::open_window(const Metrics &metrics, uint32_t z)
{
    if (!window_.open) {
        window_.open = true;
        window_.focused = true;
        window_.mode = WindowMode::Normal;
        default_geometry(metrics);
        window_.restore_x = window_.x;
        window_.restore_y = window_.y;
        window_.restore_w = window_.w;
        window_.restore_h = window_.h;
        window_.z = z;
        command_len_ = 0;
        command_history_index_ = -1;
        clear_buffer();
        write("MyOS GUI terminal\n");
        write("Commands are framebuffer-rendered.\n");
        write("Drag title, resize corner, use _ M X.\n");
        write("myos> ");
        serial_writestring("MyOS GUI: Terminal opened.\n");
        return;
    }

    if (window_.mode == WindowMode::Minimized) {
        window_.mode = WindowMode::Normal;
    }
    focus(z);
}

void TerminalApp::minimize()
{
    window_.mode = WindowMode::Minimized;
    window_.focused = false;
    serial_writestring("MyOS GUI: Terminal minimized.\n");
}

void TerminalApp::toggle_maximize(const Metrics &metrics, uint32_t z)
{
    if (window_.mode == WindowMode::Maximized) {
        window_.mode = WindowMode::Normal;
        window_.x = window_.restore_x;
        window_.y = window_.restore_y;
        window_.w = window_.restore_w;
        window_.h = window_.restore_h;
        serial_writestring("MyOS GUI: Terminal restored.\n");
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
        serial_writestring("MyOS GUI: Terminal maximized.\n");
    }
    focus(z);
}

void TerminalApp::close()
{
    window_.open = false;
    window_.focused = false;
    window_.mode = WindowMode::Normal;
    command_len_ = 0;
    serial_writestring("MyOS GUI: Terminal closed.\n");
}

void TerminalApp::focus(uint32_t z)
{
    if (!window_.open) {
        return;
    }
    window_.focused = true;
    window_.z = z;
}

void TerminalApp::scroll(int32_t delta)
{
    if (delta > 0) {
        if (scroll_ + (uint32_t) delta < line_count_) {
            scroll_ += (uint32_t) delta;
        } else {
            scroll_ = line_count_ > 0 ? line_count_ - 1 : 0;
        }
    } else if (delta < 0) {
        uint32_t amount = (uint32_t) -delta;
        scroll_ = amount > scroll_ ? 0 : scroll_ - amount;
    }
}

bool TerminalApp::hit(int32_t x, int32_t y) const
{
    return window_.visible() && window_.rect().contains(x, y);
}

void TerminalApp::draw(graphics_surface *surface) const
{
    if (!window_.visible()) {
        return;
    }

    int32_t x = window_.x;
    int32_t y = window_.y;
    int32_t w = window_.w;
    int32_t h = window_.h;
    uint32_t title_color = window_.focused ? COLOR_ACCENT : 0x00505E6A;

    graphics_fill_rect(surface, (uint32_t) x, (uint32_t) y, (uint32_t) w, (uint32_t) h, COLOR_WINDOW);
    graphics_draw_rect(surface, (uint32_t) x, (uint32_t) y, (uint32_t) w, (uint32_t) h,
                       window_.focused ? COLOR_ACCENT : COLOR_WINDOW_BORDER);
    graphics_fill_rect(surface, (uint32_t) x + 1, (uint32_t) y + 1, (uint32_t) w - 2, TITLEBAR_H, title_color);
    graphics_draw_string(surface, (uint32_t) x + 7, (uint32_t) y + 6, "MYOS TERMINAL", 15, title_color);
    graphics_fill_rect(surface, (uint32_t) x + (uint32_t) w - 48, (uint32_t) y + 4, 13, 11, 0x006D7781);
    graphics_draw_string(surface, (uint32_t) x + (uint32_t) w - 44, (uint32_t) y + 6, "_", 15, 0x006D7781);
    graphics_fill_rect(surface, (uint32_t) x + (uint32_t) w - 33, (uint32_t) y + 4, 13, 11, 0x006D7781);
    graphics_draw_string(surface, (uint32_t) x + (uint32_t) w - 30, (uint32_t) y + 6, "M", 15, 0x006D7781);
    graphics_fill_rect(surface, (uint32_t) x + (uint32_t) w - 18, (uint32_t) y + 4, 13, 11, 0x00B84A4A);
    graphics_draw_string(surface, (uint32_t) x + (uint32_t) w - 14, (uint32_t) y + 6, "X", 15, 0x00B84A4A);
    graphics_fill_rect(surface, (uint32_t) x + 4, (uint32_t) y + TITLEBAR_H + 3,
                       (uint32_t) w - 8, (uint32_t) h - TITLEBAR_H - 8, COLOR_TERMINAL_BG);

    uint32_t rows = visible_rows();
    uint32_t cols = text_cols();
    uint32_t max_start = line_count_ > rows ? line_count_ - rows : 0;
    uint32_t start = max_start > scroll_ ? max_start - scroll_ : 0;
    for (uint32_t row = 0; row < rows; row++) {
        uint32_t line = start + row;
        if (line >= line_count_) {
            break;
        }
        for (uint32_t col = 0; col < cols; col++) {
            graphics_draw_char(surface, (uint32_t) x + 8 + col * 6,
                               (uint32_t) y + TITLEBAR_H + 6 + row * 8,
                               lines_[line][col], COLOR_TERMINAL_FG, COLOR_TERMINAL_BG);
        }
    }

    if (window_.focused && ((timer_ticks() / 40) % 2) == 0 && scroll_ == 0) {
        uint32_t cursor_row = rows - 1;
        if (line_count_ < rows) {
            cursor_row = line_count_ - 1;
        }
        uint32_t cursor_col = col_ < cols ? col_ : cols - 1;
        graphics_fill_rect(surface, (uint32_t) x + 8 + cursor_col * 6,
                           (uint32_t) y + TITLEBAR_H + 6 + cursor_row * 8 + 7, 5, 1, COLOR_TERMINAL_FG);
    }

    graphics_fill_rect(surface, (uint32_t) x + (uint32_t) w - 8, (uint32_t) y + (uint32_t) h - 8, 7, 7, 0x00898F98);
    graphics_draw_string(surface, (uint32_t) x + (uint32_t) w - 8, (uint32_t) y + (uint32_t) h - 9, "/",
                         COLOR_TEXT_DARK, 0x00898F98);
}

void TerminalApp::execute_command(const char *line)
{
    if (line[0] == '\0') {
        print("myos> ");
        return;
    }

    push_history(line);
    if (kstrcmp(line, "help") == 0) {
        print("Commands: help clear about ticks uptime ls cat write delete rename truncate fsck pci run procs tasks mem\n");
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "clear") == 0) {
        clear_buffer();
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "about") == 0) {
        print("MyOS GUI double-buffered desktop.\n");
        print("Window manager: drag focus min max resize.\n");
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "ticks") == 0) {
        print_u32("Timer ticks=", timer_ticks());
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "uptime") == 0) {
        print_u32("Uptime seconds=", timer_uptime_seconds());
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "mem") == 0) {
        print_u32("PMM free pages=", pmm_free_pages());
        print_u32("Heap free bytes=", (uint32_t) heap_free_bytes());
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "tasks") == 0) {
        print_u32("Scheduler switches=", scheduler_switch_count());
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "procs") == 0) {
        print_u32("User processes=", user_process_count());
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "ls") == 0) {
        print("diskfs:\n");
        for (size_t i = 0; i < diskfs_file_count(); i++) {
            print(diskfs_file_name(i));
            print("\n");
        }
        print("myos> ");
        return;
    }
    if (line[0] == 'c' && line[1] == 'a' && line[2] == 't' && line[3] == ' ') {
        size_t size;
        const char *data = diskfs_read_file(line + 4, &size);
        if (data == 0) {
            print("File not found.\n");
        } else {
            for (size_t i = 0; i < size; i++) {
                char text[2] = {data[i], '\0'};
                print(text);
            }
            if (size == 0 || data[size - 1] != '\n') {
                print("\n");
            }
        }
        print("myos> ");
        return;
    }
    if (line[0] == 'w' && line[1] == 'r' && line[2] == 'i' && line[3] == 't' && line[4] == 'e' && line[5] == ' ') {
        const char *args = line + 6;
        char path[32];
        uint32_t len = 0;
        while (args[len] != '\0' && args[len] != ' ' && len + 1 < sizeof(path)) {
            path[len] = args[len];
            len++;
        }
        path[len] = '\0';
        if (len == 0 || args[len] != ' ' || !diskfs_write_file(path, args + len + 1, (uint32_t) kstrlen(args + len + 1))) {
            print("Write failed.\n");
        } else {
            print("Write passed.\n");
        }
        print("myos> ");
        return;
    }
    if (line[0] == 'd' && line[1] == 'e' && line[2] == 'l' && line[3] == 'e' && line[4] == 't' && line[5] == 'e' && line[6] == ' ') {
        print(diskfs_delete_file(line + 7) ? "Delete passed.\n" : "Delete failed.\n");
        print("myos> ");
        return;
    }
    if (line[0] == 'r' && line[1] == 'e' && line[2] == 'n' && line[3] == 'a' && line[4] == 'm' && line[5] == 'e' && line[6] == ' ') {
        const char *args = line + 7;
        char old_path[32];
        uint32_t len = 0;
        while (args[len] != '\0' && args[len] != ' ' && len + 1 < sizeof(old_path)) {
            old_path[len] = args[len];
            len++;
        }
        old_path[len] = '\0';
        print(len > 0 && args[len] == ' ' && diskfs_rename_file(old_path, args + len + 1) ? "Rename passed.\n" : "Rename failed.\n");
        print("myos> ");
        return;
    }
    if (line[0] == 't' && line[1] == 'r' && line[2] == 'u' && line[3] == 'n' && line[4] == 'c' && line[5] == 'a' && line[6] == 't' && line[7] == 'e' && line[8] == ' ') {
        const char *args = line + 9;
        char path[32];
        uint32_t len = 0;
        while (args[len] != '\0' && args[len] != ' ' && len + 1 < sizeof(path)) {
            path[len] = args[len];
            len++;
        }
        path[len] = '\0';
        uint32_t size = 0;
        const char *size_text = args + len + 1;
        while (*size_text >= '0' && *size_text <= '9') {
            size = size * 10 + (uint32_t) (*size_text - '0');
            size_text++;
        }
        print(len > 0 && args[len] == ' ' && diskfs_truncate_file(path, size) ? "Truncate passed.\n" : "Truncate failed.\n");
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "fsck") == 0) {
        char report[96];
        uint32_t errors = diskfs_fsck(report, sizeof(report));
        print(report);
        print_u32("fsck errors=", errors);
        print("myos> ");
        return;
    }
    if (kstrcmp(line, "pci") == 0) {
        print_u32("PCI devices=", pci_device_count());
        print("myos> ");
        return;
    }
    if (line[0] == 'r' && line[1] == 'u' && line[2] == 'n' && line[3] == ' ') {
        print("Loading user program: ");
        print(line + 4);
        print("\n");
        int code = usermode_run_program(line + 4);
        print_u32("Program exited with code ", (uint32_t) code);
        print("myos> ");
        return;
    }

    print("Unknown command. Type help.\n");
    print("myos> ");
}

void TerminalApp::recall_history(int32_t delta)
{
    if (command_history_count_ == 0) {
        return;
    }
    if (command_history_index_ < 0) {
        command_history_index_ =
            (int32_t) (command_history_count_ < COMMAND_HISTORY ? command_history_count_ : COMMAND_HISTORY) - 1;
    } else {
        command_history_index_ += delta;
    }
    int32_t max = (int32_t) (command_history_count_ < COMMAND_HISTORY ? command_history_count_ : COMMAND_HISTORY) - 1;
    if (command_history_index_ < 0) {
        command_history_index_ = 0;
    }
    if (command_history_index_ > max) {
        command_history_index_ = max;
    }

    while (command_len_ > 0) {
        command_len_--;
        write("\b");
    }
    uint32_t slot = (command_history_count_ - 1 - (uint32_t) (max - command_history_index_)) % COMMAND_HISTORY;
    command_len_ = 0;
    for (uint32_t i = 0; command_history_[slot][i] != '\0' && command_len_ + 1 < COMMAND_SIZE; i++) {
        command_line_[command_len_++] = command_history_[slot][i];
        char text[2] = {command_history_[slot][i], '\0'};
        write(text);
    }
}

void TerminalApp::handle_key(char key, const Metrics &metrics, uint32_t z)
{
    if (key == GUI_KEY_F1) {
        open_window(metrics, z);
        return;
    }
    if (key == GUI_KEY_PAGE_UP) {
        if (scroll_ + 1 < line_count_) {
            scroll_++;
        }
        return;
    }
    if (key == GUI_KEY_PAGE_DOWN) {
        if (scroll_ > 0) {
            scroll_--;
        }
        return;
    }
    if (key == GUI_KEY_ARROW_UP) {
        recall_history(-1);
        return;
    }
    if (key == GUI_KEY_ARROW_DOWN) {
        recall_history(1);
        return;
    }
    if (!window_.open || window_.mode == WindowMode::Minimized) {
        return;
    }
    focus(z);
    if (key == '\n') {
        command_line_[command_len_] = '\0';
        write("\n");
        serial_writestring("MyOS GUI terminal: command entered.\n");
        execute_command(command_line_);
        command_len_ = 0;
        return;
    }
    if (key == '\b') {
        if (command_len_ > 0) {
            command_len_--;
            write("\b");
        }
        return;
    }
    if (key >= ' ' && key <= '~' && command_len_ + 1 < COMMAND_SIZE) {
        command_line_[command_len_++] = key;
        char text[2] = {key, '\0'};
        write(text);
    }
}

} // namespace myos::gui
