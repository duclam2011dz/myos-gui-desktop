#ifndef MYOS_GUI_EVENT_H
#define MYOS_GUI_EVENT_H

#include <stdbool.h>
#include <stdint.h>

enum gui_event_type {
    GUI_EVENT_NONE,
    GUI_EVENT_KEY,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_BUTTON,
    GUI_EVENT_TIMER_TICK,
};

enum gui_key {
    GUI_KEY_F1 = 16,
    GUI_KEY_F2,
    GUI_KEY_F3,
    GUI_KEY_F4,
    GUI_KEY_F5,
    GUI_KEY_CTRL_S,
    GUI_KEY_PAGE_UP,
    GUI_KEY_PAGE_DOWN,
    GUI_KEY_ARROW_UP,
    GUI_KEY_ARROW_DOWN,
};

struct gui_event {
    enum gui_event_type type;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    char key;
    bool pressed;
    uint32_t ticks;
};

#endif
