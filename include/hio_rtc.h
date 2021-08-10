#ifndef HIO_RTC_H
#define HIO_RTC_H

typedef struct {
    int year;
    int month;
    int day;
    int hours;
    int minutes;
    int seconds;
} hio_rtc_tm_t;

int
hio_rtc_init(void);

int
hio_rtc_get(hio_rtc_tm_t *tm);

int
hio_rtc_set(const hio_rtc_tm_t *tm);

#endif
