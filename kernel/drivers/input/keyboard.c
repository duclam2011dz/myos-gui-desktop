#include "keyboard.h"

#include "scheduler.h"
#include "serial.h"
#include "vga.h"

#include <stdbool.h>

#include "gui_event.h"
#include "io.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 128
#define KEYBOARD_EVENT_BUFFER_SIZE 128

static const char scancode_ascii[128] = {
    0, 27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0, 0,
};

static const char scancode_ascii_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0,
};

static volatile char key_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t key_head;
static volatile uint32_t key_tail;
static volatile struct keyboard_event event_buffer[KEYBOARD_EVENT_BUFFER_SIZE];
static volatile uint32_t event_head;
static volatile uint32_t event_tail;
static bool shift_down;
static bool ctrl_down;
static bool alt_down;
static bool caps_lock;
static bool extended_scancode;

static bool is_letter(char c)
{
    return c >= 'a' && c <= 'z';
}

static void keyboard_buffer_push(char c)
{
    uint32_t next_head = (key_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head == key_tail) {
        return;
    }

    key_buffer[key_head] = c;
    key_head = next_head;
}

static uint8_t current_modifiers(void)
{
    uint8_t mods = 0;
    if (shift_down) {
        mods |= KEYBOARD_MOD_SHIFT;
    }
    if (ctrl_down) {
        mods |= KEYBOARD_MOD_CTRL;
    }
    if (alt_down) {
        mods |= KEYBOARD_MOD_ALT;
    }
    if (caps_lock) {
        mods |= KEYBOARD_MOD_CAPS;
    }
    return mods;
}

static void keyboard_event_push(uint16_t keycode, char ascii, bool pressed)
{
    uint32_t next_head = (event_head + 1) % KEYBOARD_EVENT_BUFFER_SIZE;
    if (next_head == event_tail) {
        return;
    }
    event_buffer[event_head].keycode = keycode;
    event_buffer[event_head].ascii = ascii;
    event_buffer[event_head].pressed = pressed;
    event_buffer[event_head].modifiers = current_modifiers();
    event_head = next_head;
}

static bool keyboard_event_pop(struct keyboard_event *out)
{
    if (event_head == event_tail) {
        return false;
    }
    *out = event_buffer[event_tail];
    event_tail = (event_tail + 1) % KEYBOARD_EVENT_BUFFER_SIZE;
    return true;
}

static bool keyboard_buffer_pop(char *out)
{
    if (key_head == key_tail) {
        return false;
    }

    *out = key_buffer[key_tail];
    key_tail = (key_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return true;
}

bool keyboard_try_read_event(struct keyboard_event *out)
{
    bool ok;
    if (out == 0) {
        return false;
    }
    __asm__ volatile ("cli");
    ok = keyboard_event_pop(out);
    __asm__ volatile ("sti");
    return ok;
}

bool keyboard_try_read_char(char *out)
{
    bool ok;
    __asm__ volatile ("cli");
    ok = keyboard_buffer_pop(out);
    __asm__ volatile ("sti");
    return ok;
}

void keyboard_initialize(void)
{
    while ((inb(KEYBOARD_STATUS_PORT) & 0x01) != 0) {
        (void) inb(KEYBOARD_DATA_PORT);
    }

    terminal_writestring("Keyboard IRQ1 enabled.\n");
    serial_writestring("MyOS keyboard: IRQ1 enabled.\n");
}

void keyboard_handle_interrupt(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }

    bool release = (scancode & 0x80) != 0;
    uint8_t code = (uint8_t) (scancode & 0x7F);
    uint16_t keycode = extended_scancode ? (uint16_t) (0xE000 | code) : code;
    extended_scancode = false;

    if (code == 0x2A || code == 0x36) {
        shift_down = !release;
        keyboard_event_push(keycode, 0, !release);
        return;
    }
    if (code == 0x1D) {
        ctrl_down = !release;
        keyboard_event_push(keycode, 0, !release);
        return;
    }
    if (code == 0x38) {
        alt_down = !release;
        keyboard_event_push(keycode, 0, !release);
        return;
    }

    if (release) {
        keyboard_event_push(keycode, 0, false);
        return;
    }

    if (code == 0x3A) {
        caps_lock = !caps_lock;
        keyboard_event_push(keycode, 0, true);
        return;
    }

    if (code == 0x49) {
        keyboard_buffer_push(GUI_KEY_PAGE_UP);
        keyboard_event_push(keycode, GUI_KEY_PAGE_UP, true);
        return;
    }
    if (code == 0x51) {
        keyboard_buffer_push(GUI_KEY_PAGE_DOWN);
        keyboard_event_push(keycode, GUI_KEY_PAGE_DOWN, true);
        return;
    }
    if (code == 0x48) {
        keyboard_buffer_push(GUI_KEY_ARROW_UP);
        keyboard_event_push(keycode, GUI_KEY_ARROW_UP, true);
        return;
    }
    if (code == 0x50) {
        keyboard_buffer_push(GUI_KEY_ARROW_DOWN);
        keyboard_event_push(keycode, GUI_KEY_ARROW_DOWN, true);
        return;
    }
    if (code == 0x3B) {
        keyboard_buffer_push(GUI_KEY_F1);
        keyboard_event_push(keycode, GUI_KEY_F1, true);
        return;
    }
    if (code == 0x3C) {
        keyboard_buffer_push(GUI_KEY_F2);
        keyboard_event_push(keycode, GUI_KEY_F2, true);
        return;
    }
    if (code == 0x3D) {
        keyboard_buffer_push(GUI_KEY_F3);
        keyboard_event_push(keycode, GUI_KEY_F3, true);
        return;
    }
    if (code == 0x3E) {
        keyboard_buffer_push(GUI_KEY_F4);
        keyboard_event_push(keycode, GUI_KEY_F4, true);
        return;
    }
    if (code == 0x3F) {
        keyboard_buffer_push(GUI_KEY_F5);
        keyboard_event_push(keycode, GUI_KEY_F5, true);
        return;
    }
    if (ctrl_down && code == 0x1F) {
        keyboard_buffer_push(GUI_KEY_CTRL_S);
        keyboard_event_push(keycode, GUI_KEY_CTRL_S, true);
        return;
    }

    char c = shift_down ? scancode_ascii_shift[code] : scancode_ascii[code];
    if (c == 0) {
        keyboard_event_push(keycode, 0, true);
        return;
    }

    if (is_letter(scancode_ascii[code]) && caps_lock != shift_down) {
        c = (char) (scancode_ascii[code] - 'a' + 'A');
    }

    keyboard_buffer_push(c);
    keyboard_event_push(keycode, c, true);
}

char keyboard_read_char(void)
{
    char c;

    for (;;) {
        __asm__ volatile ("cli");
        if (keyboard_buffer_pop(&c)) {
            __asm__ volatile ("sti");
            return c;
        }
        __asm__ volatile ("sti; hlt");
        scheduler_preempt_if_needed();
    }
}
