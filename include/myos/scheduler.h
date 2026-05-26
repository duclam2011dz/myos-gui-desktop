#ifndef MYOS_SCHEDULER_H
#define MYOS_SCHEDULER_H

#include <stdint.h>

struct interrupt_frame;

void scheduler_initialize(void);
void scheduler_on_tick(void);
void scheduler_yield(void);
void scheduler_preempt_if_needed(void);
void scheduler_request_reschedule(void);
struct interrupt_frame *scheduler_schedule_from_interrupt(struct interrupt_frame *frame);
uint32_t scheduler_task_count(void);
uint32_t scheduler_current_task(void);
uint32_t scheduler_switch_count(void);
uint32_t scheduler_preempt_pending(void);
uint32_t scheduler_task_run_count(uint32_t id);
const char *scheduler_task_name(uint32_t id);
const char *scheduler_task_state(uint32_t id);

#endif
