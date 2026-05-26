#include "util.h"

size_t kstrlen(const char *value)
{
    size_t length = 0;
    while (value[length] != '\0') {
        length++;
    }
    return length;
}

int kstrcmp(const char *left, const char *right)
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

void u32_to_hex(uint32_t value, char out[11])
{
    static const char digits[] = "0123456789ABCDEF";

    out[0] = '0';
    out[1] = 'x';
    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (uint8_t) ((value >> ((7 - i) * 4)) & 0xF);
        out[2 + i] = digits[nibble];
    }
    out[10] = '\0';
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
