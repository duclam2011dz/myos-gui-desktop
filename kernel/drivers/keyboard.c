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
static bool shift_down;
static bool ctrl_down;
static bool caps_lock;

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

static bool keyboard_buffer_pop(char *out)
{
    if (key_head == key_tail) {
        return false;
    }

    *out = key_buffer[key_tail];
    key_tail = (key_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return true;
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

    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = true;
        return;
    }
    if (scancode == 0x1D) {
        ctrl_down = true;
        return;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        shift_down = false;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_down = false;
        return;
    }

    if ((scancode & 0x80) != 0) {
        return;
    }

    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return;
    }

    if (scancode == 0x49) {
        keyboard_buffer_push(GUI_KEY_PAGE_UP);
        return;
    }
    if (scancode == 0x51) {
        keyboard_buffer_push(GUI_KEY_PAGE_DOWN);
        return;
    }
    if (scancode == 0x48) {
        keyboard_buffer_push(GUI_KEY_ARROW_UP);
        return;
    }
    if (scancode == 0x50) {
        keyboard_buffer_push(GUI_KEY_ARROW_DOWN);
        return;
    }
    if (scancode == 0x3B) {
        keyboard_buffer_push(GUI_KEY_F1);
        return;
    }
    if (scancode == 0x3C) {
        keyboard_buffer_push(GUI_KEY_F2);
        return;
    }
    if (scancode == 0x3D) {
        keyboard_buffer_push(GUI_KEY_F3);
        return;
    }
    if (scancode == 0x3E) {
        keyboard_buffer_push(GUI_KEY_F4);
        return;
    }
    if (scancode == 0x3F) {
        keyboard_buffer_push(GUI_KEY_F5);
        return;
    }
    if (ctrl_down && scancode == 0x1F) {
        keyboard_buffer_push(GUI_KEY_CTRL_S);
        return;
    }

    char c = shift_down ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
    if (c == 0) {
        return;
    }

    if (is_letter(scancode_ascii[scancode]) && caps_lock != shift_down) {
        c = (char) (scancode_ascii[scancode] - 'a' + 'A');
    }

    keyboard_buffer_push(c);
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
