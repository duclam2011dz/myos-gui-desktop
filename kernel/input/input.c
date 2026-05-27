#include "input.h"

#include "keyboard.h"
#include "mouse.h"

static bool last_left_down;
static int32_t last_mouse_x;
static int32_t last_mouse_y;
static uint32_t last_mouse_generation;

void input_initialize(void)
{
    last_left_down = mouse_left_down();
    last_mouse_x = mouse_x();
    last_mouse_y = mouse_y();
    last_mouse_generation = mouse_generation();
}

bool input_poll_event(struct gui_event *event)
{
    if (event == 0) {
        return false;
    }

    struct keyboard_event key_event;
    if (keyboard_try_read_event(&key_event)) {
        event->type = GUI_EVENT_KEY;
        event->key = key_event.ascii;
        event->keycode = key_event.keycode;
        event->modifiers = key_event.modifiers;
        event->x = last_mouse_x;
        event->y = last_mouse_y;
        event->dx = 0;
        event->dy = 0;
        event->wheel_delta = 0;
        event->pressed = key_event.pressed;
        event->ticks = 0;
        return true;
    }

    uint32_t generation = mouse_generation();
    bool left_down = mouse_left_down();
    int32_t x = mouse_x();
    int32_t y = mouse_y();

    int32_t wheel = mouse_consume_wheel_delta();
    if (wheel != 0) {
        event->type = GUI_EVENT_MOUSE_SCROLL;
        event->x = x;
        event->y = y;
        event->dx = 0;
        event->dy = 0;
        event->wheel_delta = wheel;
        event->pressed = left_down;
        event->key = 0;
        event->keycode = 0;
        event->modifiers = 0;
        event->ticks = 0;
        last_mouse_generation = generation;
        return true;
    }

    if (left_down != last_left_down) {
        last_left_down = left_down;
        last_mouse_generation = generation;
        if (!left_down) {
            int32_t unused_x;
            int32_t unused_y;
            (void) mouse_consume_click(&unused_x, &unused_y);
        }
        event->type = GUI_EVENT_MOUSE_BUTTON;
        event->x = x;
        event->y = y;
        event->dx = x - last_mouse_x;
        event->dy = y - last_mouse_y;
        event->wheel_delta = 0;
        event->pressed = left_down;
        event->key = 0;
        event->keycode = 0;
        event->modifiers = 0;
        event->ticks = 0;
        last_mouse_x = x;
        last_mouse_y = y;
        return true;
    }

    if (mouse_consume_click(&x, &y)) {
        event->type = GUI_EVENT_MOUSE_BUTTON;
        event->x = x;
        event->y = y;
        event->dx = x - last_mouse_x;
        event->dy = y - last_mouse_y;
        event->wheel_delta = 0;
        event->pressed = false;
        event->key = 0;
        event->keycode = 0;
        event->modifiers = 0;
        event->ticks = 0;
        last_left_down = false;
        last_mouse_x = x;
        last_mouse_y = y;
        last_mouse_generation = generation;
        return true;
    }

    if (generation != last_mouse_generation || x != last_mouse_x || y != last_mouse_y) {
        event->type = GUI_EVENT_MOUSE_MOVE;
        event->x = x;
        event->y = y;
        event->dx = x - last_mouse_x;
        event->dy = y - last_mouse_y;
        event->wheel_delta = 0;
        event->pressed = left_down;
        event->key = 0;
        event->keycode = 0;
        event->modifiers = 0;
        event->ticks = 0;
        last_mouse_x = x;
        last_mouse_y = y;
        last_mouse_generation = generation;
        return true;
    }

    event->type = GUI_EVENT_NONE;
    return false;
}
