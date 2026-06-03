#include "libc.h"

size_t strlen(const char *value)
{
    size_t length = 0;
    while (value[length] != '\0') {
        length++;
    }
    return length;
}

int strcmp(const char *left, const char *right)
{
    size_t i = 0;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) {
            return (unsigned char) left[i] - (unsigned char) right[i];
        }
        i++;
    }
    return (unsigned char) left[i] - (unsigned char) right[i];
}

void *memcpy(void *dst, const void *src, size_t size)
{
    unsigned char *out = (unsigned char *) dst;
    const unsigned char *in = (const unsigned char *) src;
    for (size_t i = 0; i < size; i++) {
        out[i] = in[i];
    }
    return dst;
}

void *memset(void *dst, int value, size_t size)
{
    unsigned char *out = (unsigned char *) dst;
    for (size_t i = 0; i < size; i++) {
        out[i] = (unsigned char) value;
    }
    return dst;
}

void u32_to_dec(uint32_t value, char *out, size_t out_size)
{
    char temp[21];
    size_t length = 0;

    if (out_size == 0) {
        return;
    }

    if (value == 0) {
        out[0] = '0';
        if (out_size > 1) {
            out[1] = '\0';
        }
        return;
    }

    while (value != 0 && length < sizeof(temp)) {
        temp[length++] = (char) ('0' + value % 10);
        value /= 10;
    }

    size_t i = 0;
    while (i + 1 < out_size && length > 0) {
        out[i++] = temp[--length];
    }
    out[i] = '\0';
}
