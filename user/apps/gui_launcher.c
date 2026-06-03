#include "libc.h"
#include "syscall_numbers.h"

#ifndef MYOS_GUI_LAUNCHER_APP
#define MYOS_GUI_LAUNCHER_APP MYOS_GUI_APP_TERMINAL
#endif

int main(void)
{
    return gui_open(MYOS_GUI_LAUNCHER_APP) == 0 ? 0 : 1;
}
