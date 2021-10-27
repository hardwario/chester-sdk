#ifndef CHESTER_INCLUDE_RTC_H_
#define CHESTER_INCLUDE_RTC_H_

#ifdef __cplusplus
extern "C" {
#endif

struct hio_rtc_tm {
	int year;
	int month;
	int day;
	int hours;
	int minutes;
	int seconds;
};

int hio_rtc_get(struct hio_rtc_tm *tm);
int hio_rtc_set(const struct hio_rtc_tm *tm);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_RTC_H_ */
