#include "initrd.h"

#include "util.h"

struct initrd_file {
    const char *name;
    const char *data;
    size_t size;
};

static const char hello_txt[] =
    "Hello from MyOS initrd.\n"
    "This is a tiny read-only filesystem baked into the kernel.\n";

static const char system_txt[] =
    "MyOS subsystems: E820 PMM, heap, PIT timer, keyboard, shell, ATA PIO, initrd.\n";

static struct initrd_file files[] = {
    { "hello.txt", hello_txt, sizeof(hello_txt) - 1 },
    { "system.txt", system_txt, sizeof(system_txt) - 1 },
};

void initrd_initialize(void)
{
}

size_t initrd_file_count(void)
{
    return sizeof(files) / sizeof(files[0]);
}

const char *initrd_file_name(size_t index)
{
    if (index >= initrd_file_count()) {
        return "";
    }

    return files[index].name;
}

const char *initrd_read_file(const char *name, size_t *size)
{
    for (size_t i = 0; i < initrd_file_count(); i++) {
        if (kstrcmp(name, files[i].name) == 0) {
            if (size != 0) {
                *size = files[i].size;
            }
            return files[i].data;
        }
    }

    if (size != 0) {
        *size = 0;
    }
    return 0;
}
