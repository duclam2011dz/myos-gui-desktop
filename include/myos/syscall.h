#ifndef MYOS_SYSCALL_H
#define MYOS_SYSCALL_H

#include "idt.h"

void syscall_handle(struct interrupt_frame *frame);

#endif
