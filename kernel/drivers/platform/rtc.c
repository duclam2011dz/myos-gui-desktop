#include "rtc.h"

#include "io.h"
#include "serial.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71

static bool rtc_ready;

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_INDEX, (uint8_t) (0x80 | reg));
    return inb(CMOS_DATA);
}

static bool rtc_update_in_progress(void)
{
    return (cmos_read(0x0A) & 0x80) != 0;
}

static uint8_t bcd_to_bin(uint8_t value)
{
    return (uint8_t) ((value & 0x0F) + ((value >> 4) * 10));
}

void rtc_initialize(void)
{
    rtc_ready = true;
    serial_writestring("MyOS RTC: CMOS wall clock enabled.\n");
}

bool rtc_read_datetime(struct rtc_datetime *out)
{
    if (!rtc_ready || out == 0) {
        return false;
    }
    for (uint32_t i = 0; i < 100000 && rtc_update_in_progress(); i++) {
    }

    uint8_t second = cmos_read(0x00);
    uint8_t minute = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t status_b = cmos_read(0x0B);

    if ((status_b & 0x04) == 0) {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour = (uint8_t) ((hour & 0x80) | bcd_to_bin((uint8_t) (hour & 0x7F)));
        day = bcd_to_bin(day);
        month = bcd_to_bin(month);
        year = bcd_to_bin(year);
    }
    if ((status_b & 0x02) == 0 && (hour & 0x80) != 0) {
        hour = (uint8_t) (((hour & 0x7F) + 12) % 24);
    }

    out->year = (uint16_t) (2000 + year);
    out->month = month;
    out->day = day;
    out->hour = hour;
    out->minute = minute;
    out->second = second;
    return true;
}
