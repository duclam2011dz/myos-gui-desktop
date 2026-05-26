#include "scheduler.h"

#include "idt.h"

#include <stdbool.h>
#include <stdint.h>

#define MAX_TASKS 4
#define TASK_STACK_SIZE 4096
#define TASK_QUANTUM_TICKS 20

typedef void (*task_entry_t)(void);

enum task_state {
    TASK_UNUSED,
    TASK_RUNNABLE,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_EXITED,
};

struct task {
    const char *name;
    struct interrupt_frame *frame;
    task_entry_t entry;
    uint32_t ticks;
    uint32_t runs;
    enum task_state state;
};

static struct task tasks[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(16)));
static uint32_t task_count;
static uint32_t current_task;
static uint32_t current_quantum;
static uint32_t switches;
static uint32_t preempt_pending_flag;

static void task_exit_trap(void)
{
    tasks[current_task].state = TASK_EXITED;
    scheduler_request_reschedule();
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

static void idle_task(void)
{
    for (;;) {
        __asm__ volatile ("sti; hlt");
    }
}

static void io_task(void)
{
    for (;;) {
        tasks[current_task].runs++;
        for (volatile uint32_t i = 0; i < 100000; i++) {
        }
    }
}

static struct interrupt_frame *create_initial_frame(uint32_t task_id, task_entry_t entry)
{
    uintptr_t top = (uintptr_t) (task_stacks[task_id] + TASK_STACK_SIZE);
    top &= ~((uintptr_t) 0xF);
    struct interrupt_frame *frame = (struct interrupt_frame *) (top - sizeof(struct interrupt_frame));

    uint32_t *raw = (uint32_t *) frame;
    for (uint32_t i = 0; i < sizeof(struct interrupt_frame) / sizeof(uint32_t); i++) {
        raw[i] = 0;
    }

    frame->gs = 0x10;
    frame->fs = 0x10;
    frame->es = 0x10;
    frame->ds = 0x10;
    frame->eip = (uint32_t) entry;
    frame->cs = 0x08;
    frame->eflags = 0x202;
    frame->useresp = (uint32_t) task_exit_trap;
    frame->userss = 0x10;
    return frame;
}

static void scheduler_add_task(const char *name, task_entry_t entry, bool current)
{
    if (task_count >= MAX_TASKS) {
        return;
    }

    tasks[task_count].name = name;
    tasks[task_count].entry = entry;
    tasks[task_count].ticks = 0;
    tasks[task_count].runs = 0;
    tasks[task_count].state = current ? TASK_RUNNING : TASK_RUNNABLE;
    tasks[task_count].frame = current ? 0 : create_initial_frame(task_count, entry);
    task_count++;
}

static uint32_t scheduler_pick_next(void)
{
    for (uint32_t i = 1; i <= task_count; i++) {
        uint32_t candidate = (current_task + i) % task_count;
        if (tasks[candidate].state == TASK_RUNNABLE || tasks[candidate].state == TASK_RUNNING) {
            return candidate;
        }
    }

    return current_task;
}

void scheduler_initialize(void)
{
    task_count = 0;
    current_task = 0;
    current_quantum = 0;
    switches = 0;
    preempt_pending_flag = 0;

    scheduler_add_task("shell", 0, true);
    scheduler_add_task("idle", idle_task, false);
    scheduler_add_task("io", io_task, false);
}

void scheduler_on_tick(void)
{
    if (task_count == 0) {
        return;
    }

    tasks[current_task].ticks++;
    current_quantum++;
    if (current_quantum >= TASK_QUANTUM_TICKS) {
        preempt_pending_flag = 1;
    }
}

void scheduler_request_reschedule(void)
{
    preempt_pending_flag = 1;
}

struct interrupt_frame *scheduler_schedule_from_interrupt(struct interrupt_frame *frame)
{
    if (task_count < 2 || preempt_pending_flag == 0) {
        return frame;
    }

    tasks[current_task].frame = frame;
    if (tasks[current_task].state == TASK_RUNNING) {
        tasks[current_task].state = TASK_RUNNABLE;
    }

    uint32_t previous = current_task;
    uint32_t next = scheduler_pick_next();
    if (next == previous || tasks[next].frame == 0) {
        tasks[previous].state = TASK_RUNNING;
        preempt_pending_flag = 0;
        current_quantum = 0;
        return frame;
    }

    current_task = next;
    current_quantum = 0;
    preempt_pending_flag = 0;
    switches++;
    tasks[next].runs++;
    tasks[next].state = TASK_RUNNING;
    return tasks[next].frame;
}

void scheduler_yield(void)
{
    scheduler_request_reschedule();
    __asm__ volatile ("sti; hlt");
}

void scheduler_preempt_if_needed(void)
{
    if (preempt_pending_flag != 0) {
        scheduler_yield();
    }
}

uint32_t scheduler_task_count(void)
{
    return task_count;
}

uint32_t scheduler_current_task(void)
{
    return current_task;
}

uint32_t scheduler_switch_count(void)
{
    return switches;
}

uint32_t scheduler_preempt_pending(void)
{
    return preempt_pending_flag;
}

uint32_t scheduler_task_run_count(uint32_t id)
{
    if (id >= task_count) {
        return 0;
    }

    return tasks[id].runs;
}

const char *scheduler_task_name(uint32_t id)
{
    if (id >= task_count) {
        return "";
    }

    return tasks[id].name;
}

const char *scheduler_task_state(uint32_t id)
{
    if (id >= task_count) {
        return "";
    }

    switch (tasks[id].state) {
    case TASK_RUNNABLE:
        return "runnable";
    case TASK_RUNNING:
        return "running";
    case TASK_SLEEPING:
        return "sleeping";
    case TASK_EXITED:
        return "exited";
    default:
        return "unused";
    }
}
