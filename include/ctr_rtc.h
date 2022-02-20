#ifndef CHESTER_INCLUDE_CTR_RTC_H_
#define CHESTER_INCLUDE_CTR_RTC_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_rtc_tm {
	int year;
	int month;
	int day;
	int hours;
	int minutes;
	int seconds;
};

int ctr_rtc_get_tm(struct ctr_rtc_tm *tm);
int ctr_rtc_set_tm(const struct ctr_rtc_tm *tm);
int ctr_rtc_get_ts(int64_t *ts);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_RTC_H_ */
