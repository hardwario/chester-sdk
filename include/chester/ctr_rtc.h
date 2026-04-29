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

/**
 * @addtogroup ctr_rtc ctr_rtc
 * @{
 */

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

/**
 * @brief Get current RTC timestamp in milliseconds since UNIX epoch.
 *
 * Combines the 1-second RTC tick with k_uptime_get() phase to derive
 * sub-second resolution. The returned value is the unix timestamp in
 * seconds multiplied by 1000, plus the milliseconds elapsed since the
 * last RTC second tick (capped at 999).
 *
 * Like @ref ctr_rtc_get_ts(), the result may not represent real wall
 * time if the RTC has never been set — use @ref ctr_rtc_is_synced()
 * to verify the timestamp is meaningful.
 *
 * @param[out] ts_ms milliseconds since UNIX epoch
 *
 * @retval 0 on success
 */
int ctr_rtc_get_ts_ms(int64_t *ts_ms);

/**
 * @brief Check if the RTC has been set since boot.
 *
 * The RTC defaults to 1970-01-01 00:00:00 UTC at boot. This function
 * returns true once @ref ctr_rtc_set_tm() or @ref ctr_rtc_set_ts() has
 * been called (e.g. from LTE network time sync, BLE client setup, or
 * shell command).
 *
 * @return true if the clock has been set, false otherwise
 */
bool ctr_rtc_is_synced(void);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_RTC_H_ */
