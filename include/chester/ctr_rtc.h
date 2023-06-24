/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_RTC_H_
#define CHESTER_INCLUDE_CTR_RTC_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_rtc_tm {
	/* Year in the Anno Domini calendar (e.g. 2022) */
	int year;

	/* Month of the year (range 1-12) */
	int month;

	/* Day of the month (range 1-31) */
	int day;

	/* Day of the week (range 1-7; 1 = Mon) */
	int wday;

	/* Hours since midnight (range 0-23) */
	int hours;

	/* Minutes after the hour (range 0-59) */
	int minutes;

	/* Seconds after the minute (range 0-59) */
	int seconds;
};

int ctr_rtc_get_tm(struct ctr_rtc_tm *tm);
int ctr_rtc_set_tm(const struct ctr_rtc_tm *tm);
int ctr_rtc_get_ts(int64_t *ts);
int ctr_rtc_set_ts(int64_t ts);
int ctr_rtc_wait_set(k_timeout_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_RTC_H_ */
