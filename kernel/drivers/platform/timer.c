#include "timer.h"

#include "serial.h"
#include "scheduler.h"

#include "io.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_BASE_FREQUENCY 1193182

static volatile uint32_t ticks;
static uint32_t configured_frequency;

void timer_initialize(uint32_t frequency_hz)
{
    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    configured_frequency = frequency_hz;
    ticks = 0;

    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t) (divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t) ((divisor >> 8) & 0xFF));

    serial_writestring("MyOS timer: PIT initialized at 100 Hz.\n");
}

void timer_handle_interrupt(void)
{
    ticks++;
    scheduler_on_tick();
}

uint32_t timer_ticks(void)
{
    uint32_t value;

    __asm__ volatile ("cli");
    value = ticks;
    __asm__ volatile ("sti");

    return value;
}

uint32_t timer_frequency(void)
{
    return configured_frequency;
}

uint32_t timer_uptime_seconds(void)
{
    if (configured_frequency == 0) {
        return 0;
    }

    return timer_ticks() / configured_frequency;
}
