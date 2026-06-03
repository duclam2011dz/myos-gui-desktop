#include "libc.h"

int main(void);

void _start(void)
{
    exit(main());
}
