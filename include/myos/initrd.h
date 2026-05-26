#ifndef MYOS_INITRD_H
#define MYOS_INITRD_H

#include <stddef.h>

void initrd_initialize(void);
size_t initrd_file_count(void);
const char *initrd_file_name(size_t index);
const char *initrd_read_file(const char *name, size_t *size);

#endif
