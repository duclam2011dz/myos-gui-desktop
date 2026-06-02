#include "gui_types.hpp"

#include "../apps/terminal.hpp"
#include "../apps/notepad.hpp"
#include "../apps/utility_apps.hpp"
#include "../desktop/desktop.hpp"
#include "../wm/window_manager.hpp"

extern "C" {
#include "assets.h"
#include "diskfs.h"
#include "graphics.h"
#include "gui.h"
#include "gui_event.h"
#include "heap.h"
#include "input.h"
#include "mouse.h"
#include "power.h"
#include "serial.h"
#include "timer.h"
}

namespace myos::gui {

class GuiSystem {
public:
    void initialize();
    void run();
    void terminal_write(const char *text);

private:
    Metrics metrics_ = {FALLBACK_W, FALLBACK_H, 18};
    uint8_t *backbuffer_pixels_ = nullptr;
    graphics_surface backbuffer_ = {};
    WindowManager wm_ = {};
    TerminalApp terminal_ = {};
    UtilityApp files_ = {};
    UtilityApp monitor_ = {};
    UtilityApp about_ = {};
    NotepadApp notepad_ = {};
    bool start_menu_open_ = false;
    bool dirty_ = false;
    bool full_repaint_ = false;
    Rect dirty_rect_ = {};
    int32_t mouse_press_x_ = 0;
    int32_t mouse_press_y_ = 0;
    HitResult mouse_press_hit_ = {HitTarget::None, AppId::None};
    uint32_t z_counter_ = 1;
    uint32_t last_icon_click_tick_ = 0;
    int32_t last_icon_click_x_ = -1000;
    int32_t last_icon_click_y_ = -1000;
    int32_t last_files_click_index_ = -1;
    uint32_t last_files_click_tick_ = 0;
    uint32_t input_events_since_report_ = 0;
    int32_t cursor_draw_x_ = 0;
    int32_t cursor_draw_y_ = 0;
    uint32_t cursor_text_until_tick_ = 0;

    void mark_dirty();
    void invalidate_rect(Rect rect);
    void invalidate_window(const Window &window);
    void draw();
    void flush_dirty();
    void draw_cursor(graphics_surface *surface);
    bool cursor_text_mode() const;
    int32_t cursor_width() const;
    int32_t cursor_height() const;
    void invalidate_cursor_at(int32_t x, int32_t y);
    void update_cursor_animation();
    HitResult hit_test(int32_t x, int32_t y) const;
    Window *window_for_app(AppId app);
    UtilityApp *utility_for_app(AppId app);
    NotepadApp *notepad_for_app(AppId app);
    void focus_app(AppId app);
    void open_app(AppId app);
    void minimize_app(AppId app);
    void close_app(AppId app);
    void toggle_maximize_app(AppId app);
    bool open_file_in_notepad(uint32_t file_index);
    bool open_first_text_file_in_notepad();
    bool launch_gui_program_if_supported(const char *path);
    bool run_file_from_explorer(uint32_t file_index);
    void maybe_open_file_from_explorer(int32_t x, int32_t y);
    bool click_matches_press(int32_t x, int32_t y) const;
    void handle_left_press(int32_t x, int32_t y);
    void handle_left_release(int32_t x, int32_t y);
    void handle_mouse_drag(int32_t x, int32_t y);
    void handle_event(const gui_event &event);
};

static GuiSystem g_gui;

void GuiSystem::mark_dirty()
{
    dirty_ = true;
    full_repaint_ = true;
}

void GuiSystem::invalidate_rect(Rect rect)
{
    if (rect.empty()) {
        return;
    }
    if (rect.x < 0) {
        rect.w += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.h += rect.y;
        rect.y = 0;
    }
    if (rect.x >= (int32_t) metrics_.width || rect.y >= (int32_t) metrics_.height) {
        return;
    }
    if (rect.x + rect.w > (int32_t) metrics_.width) {
        rect.w = (int32_t) metrics_.width - rect.x;
    }
    if (rect.y + rect.h > (int32_t) metrics_.height) {
        rect.h = (int32_t) metrics_.height - rect.y;
    }
    if (rect.empty()) {
        return;
    }

    if (!dirty_ || full_repaint_ || dirty_rect_.empty()) {
        dirty_rect_ = rect;
    } else {
        int32_t x1 = min_i32(dirty_rect_.x, rect.x);
        int32_t y1 = min_i32(dirty_rect_.y, rect.y);
        int32_t x2 = max_i32(dirty_rect_.x + dirty_rect_.w, rect.x + rect.w);
        int32_t y2 = max_i32(dirty_rect_.y + dirty_rect_.h, rect.y + rect.h);
        dirty_rect_ = Rect{x1, y1, x2 - x1, y2 - y1};
    }
    dirty_ = true;
}

void GuiSystem::invalidate_window(const Window &window)
{
    invalidate_rect(window.rect());
}

void GuiSystem::draw_cursor(graphics_surface *surface)
{
    (void) assets_draw_cursor(surface, cursor_draw_x_, cursor_draw_y_, cursor_text_mode());
}

bool GuiSystem::cursor_text_mode() const
{
    return (int32_t) (cursor_text_until_tick_ - timer_ticks()) > 0;
}

int32_t GuiSystem::cursor_width() const
{
    int32_t width = (int32_t) assets_cursor_width(cursor_text_mode());
    return width > 0 ? width : 24;
}

int32_t GuiSystem::cursor_height() const
{
    int32_t height = (int32_t) assets_cursor_height(cursor_text_mode());
    return height > 0 ? height : 24;
}

void GuiSystem::invalidate_cursor_at(int32_t x, int32_t y)
{
    invalidate_rect(Rect{x - 16, y - 16, cursor_width() + 32, cursor_height() + 32});
}

void GuiSystem::update_cursor_animation()
{
    int32_t target_x = mouse_x();
    int32_t target_y = mouse_y();
    int32_t dx = target_x - cursor_draw_x_;
    int32_t dy = target_y - cursor_draw_y_;
    if (dx == 0 && dy == 0) {
        return;
    }
    invalidate_cursor_at(cursor_draw_x_, cursor_draw_y_);
    int32_t abs_dx = dx < 0 ? -dx : dx;
    int32_t abs_dy = dy < 0 ? -dy : dy;
    int32_t max_delta = max_i32(abs_dx, abs_dy);
    int32_t step = max_delta / 2;
    if (step < 2) {
        step = 2;
    }
    if (step > 28) {
        step = 28;
    }
    if (abs_dx <= step) {
        cursor_draw_x_ = target_x;
    } else {
        cursor_draw_x_ += dx > 0 ? step : -step;
    }
    if (abs_dy <= step) {
        cursor_draw_y_ = target_y;
    } else {
        cursor_draw_y_ += dy > 0 ? step : -step;
    }
    invalidate_cursor_at(cursor_draw_x_, cursor_draw_y_);
}

void GuiSystem::draw()
{
    Desktop::draw(&backbuffer_, metrics_, start_menu_open_,
                  terminal_.open(), terminal_.minimized(),
                  files_.open(), files_.minimized(),
                  monitor_.open(), monitor_.minimized());
    about_.draw(&backbuffer_);
    notepad_.draw(&backbuffer_);
    monitor_.draw(&backbuffer_);
    files_.draw(&backbuffer_);
    terminal_.draw(&backbuffer_);
    draw_cursor(&backbuffer_);
}

void GuiSystem::flush_dirty()
{
    graphics_surface *screen = graphics_primary_surface_mut();
    if (screen == nullptr || !dirty_) {
        return;
    }

    draw();
    if (full_repaint_ || dirty_rect_.empty()) {
        graphics_blit(screen, &backbuffer_);
    } else {
        graphics_blit_rect(screen, &backbuffer_, (uint32_t) dirty_rect_.x, (uint32_t) dirty_rect_.y,
                           (uint32_t) dirty_rect_.w, (uint32_t) dirty_rect_.h);
    }
    dirty_ = false;
    full_repaint_ = false;
    dirty_rect_ = Rect{0, 0, 0, 0};
}

static HitTarget target_from_window_area(WindowArea area)
{
    if (area == WindowArea::Close) {
        return HitTarget::WindowClose;
    }
    if (area == WindowArea::Maximize) {
        return HitTarget::WindowMaximize;
    }
    if (area == WindowArea::Minimize) {
        return HitTarget::WindowMinimize;
    }
    if (area == WindowArea::Resize) {
        return HitTarget::WindowResize;
    }
    if (area == WindowArea::Titlebar) {
        return HitTarget::WindowTitlebar;
    }
    if (area == WindowArea::Client) {
        return HitTarget::WindowClient;
    }
    return HitTarget::None;
}

HitResult GuiSystem::hit_test(int32_t x, int32_t y) const
{
    const Window *windows[] = {&terminal_.window(), &files_.window(), &monitor_.window(), &about_.window(), &notepad_.window()};
    AppId apps[] = {AppId::Terminal, AppId::Files, AppId::Monitor, AppId::About, AppId::Notepad};
    int32_t best = -1;
    WindowArea best_area = WindowArea::None;
    AppId best_app = AppId::None;
    for (uint32_t i = 0; i < 5; i++) {
        WindowArea area = wm_.hit_window(*windows[i], x, y);
        if (area != WindowArea::None && (int32_t) windows[i]->z > best) {
            best = (int32_t) windows[i]->z;
            best_area = area;
            best_app = apps[i];
        }
    }
    if (best_app != AppId::None) {
        return HitResult{target_from_window_area(best_area), best_app};
    }

    if (Rect{4, (int32_t) metrics_.height - (int32_t) metrics_.taskbar_h + 4,
             START_BUTTON_W, (int32_t) metrics_.taskbar_h - 8}.contains(x, y)) {
        return HitResult{HitTarget::StartButton, AppId::None};
    }
    if (terminal_.open() &&
        Rect{TASK_BUTTON_X, (int32_t) metrics_.height - (int32_t) metrics_.taskbar_h + 4,
             TASK_BUTTON_W, (int32_t) metrics_.taskbar_h - 8}.contains(x, y)) {
        return HitResult{HitTarget::TaskButton, AppId::Terminal};
    }
    if (files_.open() &&
        Rect{TASK_BUTTON_X + TASK_BUTTON_W + 6, (int32_t) metrics_.height - (int32_t) metrics_.taskbar_h + 4,
             TASK_BUTTON_W, (int32_t) metrics_.taskbar_h - 8}.contains(x, y)) {
        return HitResult{HitTarget::TaskButton, AppId::Files};
    }
    if (monitor_.open() &&
        Rect{TASK_BUTTON_X + (TASK_BUTTON_W + 6) * 2, (int32_t) metrics_.height - (int32_t) metrics_.taskbar_h + 4,
             TASK_BUTTON_W, (int32_t) metrics_.taskbar_h - 8}.contains(x, y)) {
        return HitResult{HitTarget::TaskButton, AppId::Monitor};
    }
    if (start_menu_open_) {
        Rect menu = Desktop::start_menu_rect(metrics_);
        if (Rect{menu.x + 28, menu.y + 8, menu.w - 32, 18}.contains(x, y)) {
            return HitResult{HitTarget::StartTerminal, AppId::Terminal};
        }
        if (Rect{menu.x + 28, menu.y + 24, menu.w - 32, 18}.contains(x, y)) {
            return HitResult{HitTarget::StartTerminal, AppId::Files};
        }
        if (Rect{menu.x + 28, menu.y + 40, menu.w - 32, 18}.contains(x, y)) {
            return HitResult{HitTarget::StartTerminal, AppId::Monitor};
        }
        if (Rect{menu.x + 28, menu.y + 56, menu.w - 32, 18}.contains(x, y)) {
            return HitResult{HitTarget::StartTerminal, AppId::About};
        }
        if (Rect{menu.x + 28, menu.y + 84, menu.w - 32, 18}.contains(x, y)) {
            return HitResult{HitTarget::StartShutdown, AppId::None};
        }
        if (Rect{menu.x + 28, menu.y + 100, menu.w - 32, 18}.contains(x, y)) {
            return HitResult{HitTarget::StartRestart, AppId::None};
        }
    }
    if (Rect{ICON_X, ICON_Y, ICON_W, ICON_H}.contains(x, y)) {
        return HitResult{HitTarget::DesktopIcon, AppId::Terminal};
    }
    if (Rect{ICON_X, ICON_Y + 56, ICON_W, ICON_H}.contains(x, y)) {
        return HitResult{HitTarget::DesktopIcon, AppId::Files};
    }
    if (Rect{ICON_X, ICON_Y + 112, ICON_W, ICON_H}.contains(x, y)) {
        return HitResult{HitTarget::DesktopIcon, AppId::Monitor};
    }
    if (Rect{ICON_X, ICON_Y + 168, ICON_W, ICON_H}.contains(x, y)) {
        return HitResult{HitTarget::DesktopIcon, AppId::About};
    }
    return HitResult{HitTarget::Desktop, AppId::None};
}

bool GuiSystem::click_matches_press(int32_t x, int32_t y) const
{
    return x >= mouse_press_x_ - CLICK_SLOP && x <= mouse_press_x_ + CLICK_SLOP &&
           y >= mouse_press_y_ - CLICK_SLOP && y <= mouse_press_y_ + CLICK_SLOP;
}

Window *GuiSystem::window_for_app(AppId app)
{
    if (app == AppId::Terminal) {
        return &terminal_.window();
    }
    if (app == AppId::Notepad) {
        return &notepad_.window();
    }
    UtilityApp *utility = utility_for_app(app);
    return utility == nullptr ? nullptr : &utility->window();
}

NotepadApp *GuiSystem::notepad_for_app(AppId app)
{
    return app == AppId::Notepad ? &notepad_ : nullptr;
}

UtilityApp *GuiSystem::utility_for_app(AppId app)
{
    if (app == AppId::Files) {
        return &files_;
    }
    if (app == AppId::Monitor) {
        return &monitor_;
    }
    if (app == AppId::About) {
        return &about_;
    }
    return nullptr;
}

void GuiSystem::focus_app(AppId app)
{
    terminal_.window().focused = false;
    files_.window().focused = false;
    monitor_.window().focused = false;
    about_.window().focused = false;
    notepad_.window().focused = false;
    if (app == AppId::Terminal) {
        terminal_.focus(++z_counter_);
        return;
    }
    UtilityApp *utility = utility_for_app(app);
    if (utility != nullptr) {
        utility->focus(++z_counter_);
        return;
    }
    NotepadApp *notepad = notepad_for_app(app);
    if (notepad != nullptr) {
        notepad->focus(++z_counter_);
    }
}

void GuiSystem::open_app(AppId app)
{
    if (app == AppId::Terminal) {
        terminal_.open_window(metrics_, ++z_counter_);
    } else {
        UtilityApp *utility = utility_for_app(app);
        if (utility != nullptr) {
            utility->open_window(metrics_, ++z_counter_);
        }
    }
    focus_app(app);
    start_menu_open_ = false;
    mark_dirty();
}

void GuiSystem::minimize_app(AppId app)
{
    if (app == AppId::Terminal) {
        terminal_.minimize();
    } else {
        UtilityApp *utility = utility_for_app(app);
        if (utility != nullptr) {
            utility->minimize();
        }
        NotepadApp *notepad = notepad_for_app(app);
        if (notepad != nullptr) {
            notepad->minimize();
        }
    }
    mark_dirty();
}

void GuiSystem::close_app(AppId app)
{
    if (app == AppId::Terminal) {
        terminal_.close();
    } else {
        UtilityApp *utility = utility_for_app(app);
        if (utility != nullptr) {
            utility->close();
        }
        NotepadApp *notepad = notepad_for_app(app);
        if (notepad != nullptr) {
            notepad->close();
        }
    }
    wm_.end_pointer_action();
    mark_dirty();
}

void GuiSystem::toggle_maximize_app(AppId app)
{
    if (app == AppId::Terminal) {
        terminal_.toggle_maximize(metrics_, ++z_counter_);
    } else {
        UtilityApp *utility = utility_for_app(app);
        if (utility != nullptr) {
            utility->toggle_maximize(metrics_, ++z_counter_);
        }
        NotepadApp *notepad = notepad_for_app(app);
        if (notepad != nullptr) {
            notepad->toggle_maximize(metrics_, ++z_counter_);
        }
    }
    focus_app(app);
    mark_dirty();
}

bool GuiSystem::open_file_in_notepad(uint32_t file_index)
{
    if (file_index >= diskfs_file_count()) {
        return false;
    }
    bool ok = notepad_.open_file(diskfs_file_name(file_index), metrics_, ++z_counter_);
    if (ok) {
        focus_app(AppId::Notepad);
        mark_dirty();
    }
    return ok;
}

static bool file_name_looks_text(const char *name)
{
    uint32_t len = 0;
    while (name[len] != '\0') {
        len++;
    }
    if (len >= 4 && name[len - 4] == '.' && name[len - 3] == 't' && name[len - 2] == 'x' && name[len - 1] == 't') {
        return true;
    }
    return len >= 5 && name[len - 5] == '/' && name[len - 4] == 'm' && name[len - 3] == 'o' &&
           name[len - 2] == 't' && name[len - 1] == 'd';
}

static bool file_name_has_suffix(const char *name, const char *suffix)
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

static bool file_name_looks_executable(const char *name)
{
    return file_name_has_suffix(name, ".elf") || file_name_has_suffix(name, ".mx");
}

bool GuiSystem::open_first_text_file_in_notepad()
{
    for (uint32_t i = 0; i < diskfs_file_count(); i++) {
        if (file_name_looks_text(diskfs_file_name(i))) {
            return open_file_in_notepad(i);
        }
    }
    return false;
}

bool GuiSystem::launch_gui_program_if_supported(const char *path)
{
    (void) path;
    return false;
}

bool GuiSystem::run_file_from_explorer(uint32_t file_index)
{
    if (file_index >= diskfs_file_count()) {
        return false;
    }
    const char *path = diskfs_file_name(file_index);
    if (file_name_looks_text(path)) {
        return open_file_in_notepad(file_index);
    }
    if (file_name_looks_executable(path)) {
        if (launch_gui_program_if_supported(path)) {
            return true;
        }
        terminal_.open_window(metrics_, ++z_counter_);
        focus_app(AppId::Terminal);
        terminal_.run_program(path, metrics_, ++z_counter_);
        mark_dirty();
        return true;
    }
    return false;
}

void GuiSystem::maybe_open_file_from_explorer(int32_t x, int32_t y)
{
    int index = files_.file_index_at(x, y);
    if (index < 0) {
        return;
    }
    uint32_t now = timer_ticks();
    bool close_in_time = now - last_files_click_tick_ <= DOUBLE_CLICK_TICKS;
    if (close_in_time && last_files_click_index_ == index) {
        (void) run_file_from_explorer((uint32_t) index);
        last_files_click_index_ = -1;
        last_files_click_tick_ = 0;
    } else {
        last_files_click_index_ = index;
        last_files_click_tick_ = now;
        mark_dirty();
    }
}

void GuiSystem::handle_left_press(int32_t x, int32_t y)
{
    mouse_press_x_ = x;
    mouse_press_y_ = y;
    mouse_press_hit_ = hit_test(x, y);

    Window *window = window_for_app(mouse_press_hit_.app);
    if (window != nullptr && mouse_press_hit_.target != HitTarget::None) {
        focus_app(mouse_press_hit_.app);
        start_menu_open_ = false;
        mark_dirty();
    }

    if (window != nullptr && mouse_press_hit_.target == HitTarget::WindowResize) {
        wm_.begin_resize();
        return;
    }
    if (window != nullptr && mouse_press_hit_.target == HitTarget::WindowTitlebar) {
        wm_.begin_drag(*window, x, y);
        return;
    }
}

void GuiSystem::handle_left_release(int32_t x, int32_t y)
{
    if (mouse_press_hit_.target == HitTarget::None) {
        mouse_press_x_ = x;
        mouse_press_y_ = y;
        mouse_press_hit_ = hit_test(x, y);
    }
    HitResult release_hit = hit_test(x, y);
    wm_.end_pointer_action();
    if (!click_matches_press(x, y) || release_hit.target != mouse_press_hit_.target ||
        release_hit.app != mouse_press_hit_.app) {
        mouse_press_hit_ = HitResult{HitTarget::None, AppId::None};
        return;
    }

    if (release_hit.target == HitTarget::WindowClose) {
        close_app(release_hit.app);
        return;
    }
    if (release_hit.target == HitTarget::WindowMaximize) {
        toggle_maximize_app(release_hit.app);
        return;
    }
    if (release_hit.target == HitTarget::WindowMinimize) {
        minimize_app(release_hit.app);
        return;
    }
    if (release_hit.target == HitTarget::WindowClient && release_hit.app == AppId::Files) {
        maybe_open_file_from_explorer(x, y);
        return;
    }
    if (release_hit.target == HitTarget::StartButton) {
        start_menu_open_ = !start_menu_open_;
        mark_dirty();
        serial_writestring(start_menu_open_ ? "MyOS GUI: Start menu opened.\n" : "MyOS GUI: Start menu closed.\n");
        return;
    }
    if (release_hit.target == HitTarget::TaskButton || release_hit.target == HitTarget::StartTerminal) {
        open_app(release_hit.app);
        return;
    }
    if (release_hit.target == HitTarget::StartShutdown) {
        power_shutdown();
        return;
    }
    if (release_hit.target == HitTarget::StartRestart) {
        power_restart();
        return;
    }
    if (release_hit.target == HitTarget::DesktopIcon) {
        uint32_t now = timer_ticks();
        bool close_in_time = now - last_icon_click_tick_ <= DOUBLE_CLICK_TICKS;
        bool close_in_space = last_icon_click_x_ >= 0 &&
                              x >= last_icon_click_x_ - 4 && x <= last_icon_click_x_ + 4 &&
                              y >= last_icon_click_y_ - 4 && y <= last_icon_click_y_ + 4;
        if (close_in_time && close_in_space) {
            open_app(release_hit.app);
            last_icon_click_tick_ = 0;
            last_icon_click_x_ = -1000;
            last_icon_click_y_ = -1000;
        } else {
            last_icon_click_tick_ = now;
            last_icon_click_x_ = x;
            last_icon_click_y_ = y;
            serial_writestring("MyOS GUI: Terminal shortcut selected.\n");
        }
        mark_dirty();
        return;
    }

    if (release_hit.target == HitTarget::Desktop) {
        terminal_.window().focused = false;
        files_.window().focused = false;
        monitor_.window().focused = false;
        about_.window().focused = false;
        notepad_.window().focused = false;
        start_menu_open_ = false;
        mark_dirty();
    }
}

void GuiSystem::handle_mouse_drag(int32_t x, int32_t y)
{
    Window *window = window_for_app(mouse_press_hit_.app);
    if (window == nullptr) {
        return;
    }
    if (wm_.dragging()) {
        Rect before = wm_.update_drag(*window, x, y, metrics_);
        invalidate_rect(before);
        invalidate_window(*window);
    }
    if (wm_.resizing()) {
        Rect before = wm_.update_resize(*window, x, y, metrics_);
        invalidate_rect(before);
        invalidate_window(*window);
    }
}

void GuiSystem::handle_event(const gui_event &event)
{
    if (event.type == GUI_EVENT_KEY) {
        input_events_since_report_++;
        if (!event.pressed) {
            return;
        }
        if ((event.key >= ' ' && event.key <= '~') || event.key == '\b' || event.key == '\n') {
            cursor_text_until_tick_ = timer_ticks() + 80;
            invalidate_cursor_at(cursor_draw_x_, cursor_draw_y_);
        }
        if (event.key == GUI_KEY_F1) {
            open_app(AppId::Terminal);
            return;
        }
        if (event.key == GUI_KEY_F2) {
            open_app(AppId::Files);
            return;
        }
        if (event.key == GUI_KEY_F3) {
            open_app(AppId::Monitor);
            return;
        }
        if (event.key == GUI_KEY_F4) {
            open_app(AppId::About);
            return;
        }
        if (event.key == GUI_KEY_F5) {
            open_app(AppId::Files);
            (void) open_first_text_file_in_notepad();
            return;
        }
        if (notepad_.open() && notepad_.window().focused) {
            notepad_.handle_key(event.key);
            mark_dirty();
            return;
        }
        terminal_.handle_key(event.key, metrics_, ++z_counter_);
        if (terminal_.open() && !terminal_.minimized()) {
            files_.window().focused = false;
            monitor_.window().focused = false;
            about_.window().focused = false;
        }
        mark_dirty();
        return;
    }
    if (event.type == GUI_EVENT_MOUSE_SCROLL) {
        input_events_since_report_++;
        if (terminal_.open() && terminal_.window().focused) {
            terminal_.scroll(event.wheel_delta > 0 ? -1 : 1);
        }
        mark_dirty();
        return;
    }
    if (event.type == GUI_EVENT_MOUSE_BUTTON) {
        input_events_since_report_++;
        if (event.pressed) {
            handle_left_press(event.x, event.y);
        } else {
            handle_left_release(event.x, event.y);
        }
        return;
    }
    if (event.type == GUI_EVENT_MOUSE_MOVE) {
        input_events_since_report_++;
        if (event.pressed) {
            handle_mouse_drag(event.x, event.y);
        }
        invalidate_cursor_at(cursor_draw_x_, cursor_draw_y_);
        return;
    }
    if (event.type == GUI_EVENT_TIMER_TICK) {
        mark_dirty();
    }
}

void GuiSystem::terminal_write(const char *text)
{
    terminal_.write(text);
    mark_dirty();
}

void GuiSystem::initialize()
{
    graphics_surface *screen = graphics_primary_surface_mut();
    if (screen != nullptr) {
        metrics_.width = screen->width;
        metrics_.height = screen->height;
        metrics_.taskbar_h = metrics_.height >= 480 ? 30 : 22;
        uint32_t buffer_size = screen->pitch * screen->height;
        backbuffer_pixels_ = (uint8_t *) kmalloc(buffer_size);
        if (backbuffer_pixels_ != nullptr) {
            backbuffer_.pixels = backbuffer_pixels_;
            backbuffer_.width = screen->width;
            backbuffer_.height = screen->height;
            backbuffer_.pitch = screen->pitch;
            backbuffer_.bits_per_pixel = screen->bits_per_pixel;
            backbuffer_.format = screen->format;
        } else {
            backbuffer_ = *screen;
        }
        mouse_set_bounds((int32_t) metrics_.width, (int32_t) metrics_.height);
    }

    terminal_.initialize(metrics_);
    files_.initialize(UtilityKind::Files, metrics_);
    monitor_.initialize(UtilityKind::Monitor, metrics_);
    about_.initialize(UtilityKind::About, metrics_);
    notepad_.initialize(metrics_);
    start_menu_open_ = false;
    wm_.end_pointer_action();
    mouse_press_hit_ = HitResult{HitTarget::None, AppId::None};
    dirty_ = true;
    full_repaint_ = true;
    dirty_rect_ = Rect{0, 0, 0, 0};
    last_icon_click_tick_ = 0;
    last_icon_click_x_ = -1000;
    last_icon_click_y_ = -1000;
    last_files_click_index_ = -1;
    last_files_click_tick_ = 0;
    input_events_since_report_ = 0;
    assets_initialize();
    cursor_draw_x_ = mouse_x();
    cursor_draw_y_ = mouse_y();
    cursor_text_until_tick_ = 0;

    serial_writestring("MyOS GUI: desktop initialized with no open apps.\n");
    serial_writestring("MyOS GUI: double buffering enabled.\n");
    if (metrics_.width > FALLBACK_W || metrics_.height > FALLBACK_H) {
        serial_writestring("MyOS graphics: VBE linear framebuffer enabled.\n");
    }
    input_initialize();
    flush_dirty();
}

void GuiSystem::run()
{
    uint32_t last_second = timer_uptime_seconds();
    uint32_t last_cursor_tick = timer_ticks();
    uint32_t last_input_report = timer_ticks();
    for (;;) {
        gui_event event;
        uint32_t drained = 0;
        while (input_poll_event(&event) && drained < 64) {
            handle_event(event);
            drained++;
        }
        update_cursor_animation();

        uint32_t second = timer_uptime_seconds();
        if (second != last_second) {
            last_second = second;
            event.type = GUI_EVENT_TIMER_TICK;
            event.ticks = timer_ticks();
            handle_event(event);
        }

        uint32_t tick = timer_ticks();
        if (tick - last_cursor_tick >= 20) {
            last_cursor_tick = tick;
            invalidate_cursor_at(cursor_draw_x_, cursor_draw_y_);
        }

        if (tick - last_input_report >= 100) {
            if (input_events_since_report_ > 0) {
                serial_writestring("MyOS GUI: input batch processed.\n");
            }
            input_events_since_report_ = 0;
            last_input_report = tick;
        }

        flush_dirty();
        __asm__ volatile ("sti; hlt");
    }
}

} // namespace myos::gui

extern "C" void gui_initialize(void)
{
    myos::gui::g_gui.initialize();
}

extern "C" void gui_run(void)
{
    myos::gui::g_gui.run();
}

extern "C" void gui_terminal_write(const char *text)
{
    myos::gui::g_gui.terminal_write(text);
}
