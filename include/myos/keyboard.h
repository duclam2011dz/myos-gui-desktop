#ifndef MYOS_KEYBOARD_H
#define MYOS_KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_MOD_SHIFT 0x01
#define KEYBOARD_MOD_CTRL 0x02
#define KEYBOARD_MOD_ALT 0x04
#define KEYBOARD_MOD_CAPS 0x08

struct keyboard_event {
    uint16_t keycode;
    char ascii;
    bool pressed;
    uint8_t modifiers;
};

void keyboard_initialize(void);
void keyboard_handle_interrupt(void);
bool keyboard_try_read_event(struct keyboard_event *out);
bool keyboard_try_read_char(char *out);
char keyboard_read_char(void);

#endif
