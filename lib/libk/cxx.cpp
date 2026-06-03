extern "C" void __cxa_pure_virtual()
{
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

extern "C" {
void *__dso_handle = nullptr;
}
