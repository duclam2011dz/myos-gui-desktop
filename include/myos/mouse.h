#ifndef MYOS_MOUSE_H
#define MYOS_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

void mouse_initialize(void);
void mouse_set_bounds(int32_t width, int32_t height);
void mouse_handle_interrupt(void);
int32_t mouse_x(void);
int32_t mouse_y(void);
bool mouse_left_down(void);
uint32_t mouse_generation(void);
bool mouse_consume_click(int32_t *x, int32_t *y);

#endif
