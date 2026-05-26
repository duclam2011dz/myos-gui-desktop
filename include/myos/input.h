#ifndef MYOS_INPUT_H
#define MYOS_INPUT_H

#include <stdbool.h>

#include "gui_event.h"

void input_initialize(void);
bool input_poll_event(struct gui_event *event);

#endif
