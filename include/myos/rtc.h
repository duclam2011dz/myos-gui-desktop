#ifndef MYOS_RTC_H
#define MYOS_RTC_H

#include <stdbool.h>
#include <stdint.h>

struct rtc_datetime {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

void rtc_initialize(void);
bool rtc_read_datetime(struct rtc_datetime *out);

#endif
