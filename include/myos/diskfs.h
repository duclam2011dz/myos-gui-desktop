#ifndef MYOS_DISKFS_H
#define MYOS_DISKFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void diskfs_initialize(uint32_t start_lba);
bool diskfs_is_available(void);
size_t diskfs_file_count(void);
const char *diskfs_file_name(size_t index);
uint32_t diskfs_file_size(size_t index);
int diskfs_find_file(const char *path);
bool diskfs_read_index(uint32_t index, uint32_t offset, void *buffer, uint32_t size, uint32_t *bytes_read);
bool diskfs_write_file(const char *path, const void *buffer, uint32_t size);
const char *diskfs_read_file(const char *name, size_t *size);
uint32_t diskfs_max_file_size(void);
bool diskfs_validate(void);

#endif
