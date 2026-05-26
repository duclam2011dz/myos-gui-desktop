#ifndef MYOS_KEYBOARD_H
#define MYOS_KEYBOARD_H

#include <stdbool.h>

void keyboard_initialize(void);
void keyboard_handle_interrupt(void);
bool keyboard_try_read_char(char *out);
char keyboard_read_char(void);

#endif
