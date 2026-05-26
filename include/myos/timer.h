#ifndef MYOS_TIMER_H
#define MYOS_TIMER_H

#include <stdint.h>

void timer_initialize(uint32_t frequency_hz);
void timer_handle_interrupt(void);
uint32_t timer_ticks(void);
uint32_t timer_frequency(void);
uint32_t timer_uptime_seconds(void);

#endif
