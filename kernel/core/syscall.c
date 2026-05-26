#include "syscall.h"

#include "diskfs.h"
#include "paging.h"
#include "scheduler.h"
#include "serial.h"
#include "timer.h"
#include "usermode.h"
#include "vga.h"

#include <stdbool.h>
#include <stdint.h>

#define SYSCALL_WRITE 1
#define SYSCALL_EXIT 2
#define SYSCALL_YIELD 3
#define SYSCALL_UPTIME 4
#define SYSCALL_OPEN 5
#define SYSCALL_READ 6
#define SYSCALL_CLOSE 7
#define SYSCALL_GETPID 8
#define SYSCALL_WAITPID 9
#define SYSCALL_WRITEFILE 10

extern void usermode_return_from_exit(void);
extern uint32_t usermode_exit_code;

static uint32_t syscall_yield_count;

static bool user_buffer_is_valid(uint32_t ptr, uint32_t size)
{
    if (ptr == 0 || size == 0 || size > 4096) {
        return false;
    }

    if (ptr + size < ptr) {
        return false;
    }

    return paging_get_physical(ptr) != 0 && paging_get_physical(ptr + size - 1) != 0;
}

static void copy_from_user(char *dst, const char *src, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

static void syscall_write(const char *data, uint32_t size)
{
    terminal_write(data, size);
    for (uint32_t i = 0; i < size; i++) {
        serial_write_char(data[i]);
    }
}

void syscall_handle(struct interrupt_frame *frame)
{
    if (frame->eax == SYSCALL_WRITE) {
        if (user_buffer_is_valid(frame->ebx, frame->ecx)) {
            syscall_write((const char *) frame->ebx, frame->ecx);
            frame->eax = frame->ecx;
        } else {
            frame->eax = (uint32_t) -1;
        }
        return;
    }

    if (frame->eax == SYSCALL_EXIT) {
        usermode_exit_code = frame->ebx;
        frame->eip = (uint32_t) usermode_return_from_exit;
        frame->cs = 0x08;
        frame->eflags = 0x202;
        return;
    }

    if (frame->eax == SYSCALL_YIELD) {
        syscall_yield_count++;
        scheduler_request_reschedule();
        frame->eax = 0;
        return;
    }

    if (frame->eax == SYSCALL_UPTIME) {
        frame->eax = timer_uptime_seconds();
        return;
    }

    if (frame->eax == SYSCALL_OPEN) {
        char name[32];
        if (!user_buffer_is_valid(frame->ebx, frame->ecx) || frame->ecx == 0 || frame->ecx >= sizeof(name)) {
            frame->eax = (uint32_t) -1;
            return;
        }

        copy_from_user(name, (const char *) frame->ebx, frame->ecx);
        name[frame->ecx] = '\0';

        frame->eax = (uint32_t) user_process_fd_open(name);
        return;
    }

    if (frame->eax == SYSCALL_READ) {
        uint32_t fd = frame->ebx;
        uint32_t size = frame->edx;
        if (!user_buffer_is_valid(frame->ecx, size)) {
            frame->eax = (uint32_t) -1;
            return;
        }

        frame->eax = (uint32_t) user_process_fd_read(fd, (char *) frame->ecx, size);
        return;
    }

    if (frame->eax == SYSCALL_CLOSE) {
        frame->eax = (uint32_t) user_process_fd_close(frame->ebx);
        return;
    }

    if (frame->eax == SYSCALL_GETPID) {
        frame->eax = user_process_current_pid();
        return;
    }

    if (frame->eax == SYSCALL_WAITPID) {
        frame->eax = (uint32_t) user_process_wait_pid(frame->ebx);
        return;
    }

    if (frame->eax == SYSCALL_WRITEFILE) {
        char name[32];
        if (!user_buffer_is_valid(frame->ebx, frame->ecx) || frame->ecx == 0 || frame->ecx >= sizeof(name) ||
            !user_buffer_is_valid(frame->edx, frame->esi)) {
            frame->eax = (uint32_t) -1;
            return;
        }

        copy_from_user(name, (const char *) frame->ebx, frame->ecx);
        name[frame->ecx] = '\0';
        frame->eax = diskfs_write_file(name, (const void *) frame->edx, frame->esi) ? 0 : (uint32_t) -2;
        return;
    }

    frame->eax = (uint32_t) -255;
}
