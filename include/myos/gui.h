#ifndef MYOS_GUI_H
#define MYOS_GUI_H

#include <stdint.h>

void gui_initialize(void);
void gui_run(void);
void gui_terminal_write(const char *text);
int gui_open_application(uint32_t app_id);

#endif
