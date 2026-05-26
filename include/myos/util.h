#ifndef MYOS_UTIL_H
#define MYOS_UTIL_H

#include <stddef.h>
#include <stdint.h>

size_t kstrlen(const char *value);
int kstrcmp(const char *left, const char *right);
void u32_to_hex(uint32_t value, char out[11]);
void u32_to_dec(uint32_t value, char *out, size_t out_size);

#endif
