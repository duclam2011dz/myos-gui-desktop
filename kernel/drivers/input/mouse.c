#include "mouse.h"

#include "serial.h"

#include "io.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

static int32_t cursor_x = 160;
static int32_t cursor_y = 100;
static int32_t bound_width = 320;
static int32_t bound_height = 200;
static bool left_down_flag;
static bool right_down_flag;
static bool middle_down_flag;
static bool click_pending;
static int32_t click_x;
static int32_t click_y;
static int32_t wheel_delta;
static uint32_t movement_generation;
static uint8_t packet[4];
static uint8_t packet_index;
static uint8_t packet_size = 3;

static void mouse_wait_input(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) {
            return;
        }
    }
}

static void mouse_wait_output(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x01) != 0) {
            return;
        }
    }
}

static void mouse_write(uint8_t value)
{
    mouse_wait_input();
    outb(PS2_COMMAND, 0xD4);
    mouse_wait_input();
    outb(PS2_DATA, value);
}

static uint8_t mouse_read_response(void)
{
    mouse_wait_output();
    return inb(PS2_DATA);
}

static void mouse_set_sample_rate(uint8_t rate)
{
    mouse_write(0xF3);
    (void) mouse_read_response();
    mouse_write(rate);
    (void) mouse_read_response();
}

void mouse_initialize(void)
{
    mouse_wait_input();
    outb(PS2_COMMAND, 0xA8);

    mouse_wait_input();
    outb(PS2_COMMAND, 0x20);
    mouse_wait_output();
    uint8_t status = inb(PS2_DATA) | 0x02;
    mouse_wait_input();
    outb(PS2_COMMAND, 0x60);
    mouse_wait_input();
    outb(PS2_DATA, status);

    mouse_write(0xF6);
    (void) mouse_read_response();
    mouse_set_sample_rate(200);
    mouse_set_sample_rate(100);
    mouse_set_sample_rate(80);
    mouse_write(0xF2);
    (void) mouse_read_response();
    uint8_t id = mouse_read_response();
    if (id == 3 || id == 4) {
        packet_size = 4;
        serial_writestring("MyOS mouse: IntelliMouse wheel packets enabled.\n");
    }
    mouse_write(0xF4);
    (void) mouse_read_response();

    serial_writestring("MyOS mouse: PS/2 mouse enabled.\n");
}

void mouse_set_bounds(int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    bound_width = width;
    bound_height = height;
    cursor_x = width / 2;
    cursor_y = height / 2;
    movement_generation++;
}

void mouse_handle_interrupt(void)
{
    uint8_t data = inb(PS2_DATA);
    if (packet_index == 0 && (data & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = data;
    if (packet_index < packet_size) {
        return;
    }
    packet_index = 0;

    bool was_left = left_down_flag;
    left_down_flag = (packet[0] & 0x01) != 0;
    right_down_flag = (packet[0] & 0x02) != 0;
    middle_down_flag = (packet[0] & 0x04) != 0;

    int32_t dx = (int32_t) (int8_t) packet[1];
    int32_t dy = (int32_t) (int8_t) packet[2];
    cursor_x += dx;
    cursor_y -= dy;

    if (cursor_x < 0) {
        cursor_x = 0;
    }
    if (cursor_y < 0) {
        cursor_y = 0;
    }
    if (cursor_x >= bound_width) {
        cursor_x = bound_width - 1;
    }
    if (cursor_y >= bound_height) {
        cursor_y = bound_height - 1;
    }
    movement_generation++;

    if (packet_size == 4) {
        int8_t wheel = (int8_t) (packet[3] & 0x0F);
        if ((wheel & 0x08) != 0) {
            wheel |= (int8_t) 0xF0;
        }
        if (wheel != 0) {
            wheel_delta += wheel;
            movement_generation++;
        }
    }

    if (was_left && !left_down_flag) {
        click_pending = true;
        click_x = cursor_x;
        click_y = cursor_y;
        movement_generation++;
    }
}

int32_t mouse_x(void)
{
    return cursor_x;
}

int32_t mouse_y(void)
{
    return cursor_y;
}

bool mouse_left_down(void)
{
    return left_down_flag;
}

bool mouse_right_down(void)
{
    return right_down_flag;
}

bool mouse_middle_down(void)
{
    return middle_down_flag;
}

int32_t mouse_consume_wheel_delta(void)
{
    int32_t delta = wheel_delta;
    wheel_delta = 0;
    return delta;
}

uint32_t mouse_generation(void)
{
    return movement_generation;
}

bool mouse_consume_click(int32_t *x, int32_t *y)
{
    if (!click_pending) {
        return false;
    }

    click_pending = false;
    if (x != 0) {
        *x = click_x;
    }
    if (y != 0) {
        *y = click_y;
    }
    return true;
}
