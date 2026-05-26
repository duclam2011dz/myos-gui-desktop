#ifndef MYOS_USERMODE_H
#define MYOS_USERMODE_H

#include <stddef.h>
#include <stdint.h>

int usermode_run_test(void);
int usermode_run_program(const char *name);
uint32_t user_process_count(void);
uint32_t user_process_pid(uint32_t index);
const char *user_process_name(uint32_t index);
const char *user_process_state(uint32_t index);
int user_process_exit_code(uint32_t index);
uint32_t user_process_current_pid(void);
int user_process_wait_pid(uint32_t pid);
uint32_t user_process_reap_exited(void);
int user_process_fd_open(const char *path);
int user_process_fd_read(uint32_t fd, char *buffer, uint32_t size);
int user_process_fd_close(uint32_t fd);
void user_process_close_all(void);

#endif
