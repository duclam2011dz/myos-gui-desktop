#include "libc.h"

int main(void)
{
    char buffer[96];
    int fd;
    int bytes;

    write(1, "Hello from disk-loaded Ring 3 ELF via libc.\n", 44);

    fd = open("hello.txt");
    if (fd < 0) {
        write(1, "open hello.txt failed\n", 22);
        return 2;
    }

    bytes = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes < 0) {
        write(1, "read hello.txt failed\n", 22);
        return 3;
    }

    buffer[bytes] = '\0';
    write(1, "Read via libc read: ", 20);
    write(1, buffer, (size_t) bytes);
    if (bytes == 0 || buffer[bytes - 1] != '\n') {
        write(1, "\n", 1);
    }
    return 7;
}
