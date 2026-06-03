#include "libc.h"

#include "syscall_numbers.h"

static inline int syscall0(uint32_t number)
{
    uint32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number) : "memory");
    return (int) result;
}

static inline int syscall1(uint32_t number, uint32_t a)
{
    uint32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(a) : "memory");
    return (int) result;
}

static inline int syscall2(uint32_t number, uint32_t a, uint32_t b)
{
    uint32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(a), "c"(b) : "memory");
    return (int) result;
}

static inline int syscall3(uint32_t number, uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(a), "c"(b), "d"(c) : "memory");
    return (int) result;
}

static inline int syscall4(uint32_t number, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    uint32_t result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
    return (int) result;
}

int write(int fd, const void *buffer, size_t size)
{
    (void) fd;
    return syscall2(MYOS_SYSCALL_WRITE, (uint32_t) buffer, (uint32_t) size);
}

void exit(int code)
{
    (void) syscall1(MYOS_SYSCALL_EXIT, (uint32_t) code);
    for (;;) {
    }
}

int yield(void)
{
    return syscall0(MYOS_SYSCALL_YIELD);
}

uint32_t uptime(void)
{
    return (uint32_t) syscall0(MYOS_SYSCALL_UPTIME);
}

int open(const char *path)
{
    return syscall2(MYOS_SYSCALL_OPEN, (uint32_t) path, (uint32_t) strlen(path));
}

int read(int fd, void *buffer, size_t size)
{
    return syscall3(MYOS_SYSCALL_READ, (uint32_t) fd, (uint32_t) buffer, (uint32_t) size);
}

int close(int fd)
{
    return syscall1(MYOS_SYSCALL_CLOSE, (uint32_t) fd);
}

uint32_t getpid(void)
{
    return (uint32_t) syscall0(MYOS_SYSCALL_GETPID);
}

int waitpid(uint32_t pid)
{
    return syscall1(MYOS_SYSCALL_WAITPID, pid);
}

int writefile(const char *path, const void *buffer, size_t size)
{
    return syscall4(MYOS_SYSCALL_WRITEFILE, (uint32_t) path, (uint32_t) strlen(path), (uint32_t) buffer, (uint32_t) size);
}

int gui_open(uint32_t app_id)
{
    return syscall1(MYOS_SYSCALL_GUI_OPEN, app_id);
}
