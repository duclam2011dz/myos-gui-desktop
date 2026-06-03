#ifndef MYOS_USER_LIBC_H
#define MYOS_USER_LIBC_H

#include <stddef.h>
#include <stdint.h>

int write(int fd, const void *buffer, size_t size);
void exit(int code) __attribute__((noreturn));
int yield(void);
uint32_t uptime(void);
int open(const char *path);
int read(int fd, void *buffer, size_t size);
int close(int fd);
uint32_t getpid(void);
int waitpid(uint32_t pid);
int writefile(const char *path, const void *buffer, size_t size);
int gui_open(uint32_t app_id);

size_t strlen(const char *value);
int strcmp(const char *left, const char *right);
void *memcpy(void *dst, const void *src, size_t size);
void *memset(void *dst, int value, size_t size);
void u32_to_dec(uint32_t value, char *out, size_t out_size);

#endif
