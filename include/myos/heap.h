#ifndef MYOS_HEAP_H
#define MYOS_HEAP_H

#include <stddef.h>
#include <stdint.h>

void heap_initialize(void);
void *kmalloc(size_t size);
void *kmalloc_aligned(size_t size, size_t alignment);
void kfree(void *ptr);
size_t heap_total_bytes(void);
size_t heap_used_bytes(void);
size_t heap_free_bytes(void);
uint32_t heap_invalid_free_count(void);
uint32_t heap_double_free_count(void);

#endif
