#ifndef MYOS_SERIAL_H
#define MYOS_SERIAL_H

void serial_initialize(void);
void serial_write_char(char c);
void serial_writestring(const char *data);

#endif
