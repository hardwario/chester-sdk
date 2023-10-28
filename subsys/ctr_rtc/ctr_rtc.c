/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/timeutil.h>

#include <nrfx_rtc.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

LOG_MODULE_REGISTER(ctr_rtc, CONFIG_CTR_RTC_LOG_LEVEL);

#define EVENT_RTC_SET BIT(0)

static K_EVENT_DEFINE(m_rtc_events);

static const nrfx_rtc_t m_rtc = NRFX_RTC_INSTANCE(2);

static struct onoff_client m_lfclk_cli;

static int m_year = 1970;
static int m_month = 1;
static int m_day = 1;
static int m_wday = 4;
static int m_hours = 0;
static int m_minutes = 0;
static int m_seconds = 0;

static int get_days_in_month(int year, int month)
{
	if (month < 1 || month > 12) {
		return 0;
	}

	if (month == 4 || month == 6 || month == 9 || month == 11) {
		return 30;

	} else if (month == 2) {
		if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0)) {
			return 29;

		} else {
			return 28;
		}
	}

	return 31;
}

static int get_day_of_week(int year, int month, int day)
{
	int adjustment = (14 - month) / 12;
	int m = month + 12 * adjustment - 2;
	int y = year - adjustment;
	int w = (day + (13 * m - 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7;

	return w ? w : 7;
}

int ctr_rtc_get_tm(struct ctr_rtc_tm *tm)
{
	irq_disable(RTC2_IRQn);

	tm->year = m_year;
	tm->month = m_month;
	tm->day = m_day;
	tm->wday = m_wday;
	tm->hours = m_hours;
	tm->minutes = m_minutes;
	tm->seconds = m_seconds;

	irq_enable(RTC2_IRQn);

	return 0;
}

int ctr_rtc_set_tm(const struct ctr_rtc_tm *tm)
{
	if (tm->year < 1970 || tm->year > 2099) {
		return -EINVAL;
	}

	if (tm->month < 1 || tm->month > 12) {
		return -EINVAL;
	}

	if (tm->day < 1 || tm->day > get_days_in_month(tm->year, tm->month)) {
		return -EINVAL;
	}

	if (tm->hours < 0 || tm->hours > 23) {
		return -EINVAL;
	}

	if (tm->minutes < 0 || tm->minutes > 59) {
		return -EINVAL;
	}

	if (tm->seconds < 0 || tm->seconds > 59) {
		return -EINVAL;
	}

	irq_disable(RTC2_IRQn);

	m_year = tm->year;
	m_month = tm->month;
	m_day = tm->day;
	m_wday = get_day_of_week(m_year, m_month, m_day);
	m_hours = tm->hours;
	m_minutes = tm->minutes;
	m_seconds = tm->seconds;

	irq_enable(RTC2_IRQn);

	k_event_post(&m_rtc_events, EVENT_RTC_SET);

	return 0;
}

int ctr_rtc_get_ts(int64_t *ts)
{
	struct tm tm = {0};

	irq_disable(RTC2_IRQn);

	tm.tm_year = m_year - 1900;
	tm.tm_mon = m_month - 1;
	tm.tm_mday = m_day;
	tm.tm_hour = m_hours;
	tm.tm_min = m_minutes;
	tm.tm_sec = m_seconds;
	tm.tm_isdst = -1;

	irq_enable(RTC2_IRQn);

	*ts = timeutil_timegm64(&tm);

	return 0;
}

int ctr_rtc_set_ts(int64_t ts)
{
	int ret;

	time_t time = ts;
	struct tm result = {0};
	if (!gmtime_r(&time, &result)) {
		LOG_ERR("Call `gmtime_r` failed");
		return -EINVAL;
	}

	struct ctr_rtc_tm tm = {
		.year = result.tm_year + 1900,
		.month = result.tm_mon + 1,
		.day = result.tm_mday,
		.hours = result.tm_hour,
		.minutes = result.tm_min,
		.seconds = result.tm_sec,
	};

	ret = ctr_rtc_set_tm(&tm);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_set_tm` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_rtc_wait_set(k_timeout_t timeout)
{
	if (k_event_wait(&m_rtc_events, EVENT_RTC_SET, false, K_NO_WAIT)) {
		return 0;
	}

	LOG_INF("Waiting for RTC to be set");

	if (k_event_wait(&m_rtc_events, EVENT_RTC_SET, false, timeout)) {
		LOG_INF("RTC has been set - continuing");
		return 0;
	}

	return -ETIMEDOUT;
}

static int cmd_rtc_get(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	struct ctr_rtc_tm tm;
	ret = ctr_rtc_get_tm(&tm);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_tm` failed: %d", ret);
		return ret;
	}

	static const char *wday[] = {
		"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",
	};

	shell_print(shell, "%04d/%02d/%02d %02d:%02d:%02d %s", tm.year, tm.month, tm.day, tm.hours,
		    tm.minutes, tm.seconds, wday[tm.wday - 1]);

	return 0;
}

static int cmd_rtc_set(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	char *date = argv[1];
	char *time = argv[2];

	/* clang-format off */

	if (strlen(date) != 10 ||
	    !isdigit((unsigned char)date[0]) ||
	    !isdigit((unsigned char)date[1]) ||
	    !isdigit((unsigned char)date[2]) ||
	    !isdigit((unsigned char)date[3]) ||
	    date[4] != '/' ||
	    !isdigit((unsigned char)date[5]) ||
	    !isdigit((unsigned char)date[6]) ||
	    date[7] != '/' ||
	    !isdigit((unsigned char)date[8]) ||
	    !isdigit((unsigned char)date[9])) {
		shell_help(shell);
		return -EINVAL;
	}

	if (strlen(time) != 8 ||
	    !isdigit((unsigned char)time[0]) ||
	    !isdigit((unsigned char)time[1]) ||
	    time[2] != ':' ||
	    !isdigit((unsigned char)time[3]) ||
	    !isdigit((unsigned char)time[4]) ||
	    time[5] != ':' ||
	    !isdigit((unsigned char)time[6]) ||
	    !isdigit((unsigned char)time[7])) {
		shell_help(shell);
		return -EINVAL;
	}

	/* clang-format on */

	date[4] = '\0';
	date[7] = '\0';
	time[2] = '\0';
	time[5] = '\0';

	struct ctr_rtc_tm tm;

	tm.year = strtoll(&date[0], NULL, 10);
	tm.month = strtoll(&date[5], NULL, 10);
	tm.day = strtoll(&date[8], NULL, 10);
	tm.hours = strtoll(&time[0], NULL, 10);
	tm.minutes = strtoll(&time[3], NULL, 10);
	tm.seconds = strtoll(&time[6], NULL, 10);

	ret = ctr_rtc_set_tm(&tm);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_set_tm` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_rtc,

	SHELL_CMD_ARG(get, NULL,
	              "Get current date/time (format YYYY/MM/DD hh:mm:ss).",
	              cmd_rtc_get, 1, 0),

	SHELL_CMD_ARG(set, NULL,
	              "Set current date/time (format YYYY/MM/DD hh:mm:ss).",
	              cmd_rtc_set, 3, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(rtc, &sub_rtc, "RTC commands for date/time operations.", print_help);

/* clang-format on */

static void rtc_handler(nrfx_rtc_int_type_t int_type)
{
	if (int_type != NRFX_RTC_INT_TICK) {
		return;
	}

	static int prescaler = 0;

	if (++prescaler % 8 != 0) {
		return;
	}

	prescaler = 0;

	if (++m_seconds < 60) {
		return;
	}

	m_seconds = 0;

	if (++m_minutes < 60) {
		return;
	}

	m_minutes = 0;

	if (++m_hours < 24) {
		return;
	}

	m_hours = 0;

	if (++m_wday >= 8) {
		m_wday = 1;
	}

	if (++m_day <= get_days_in_month(m_year, m_month)) {
		return;
	}

	m_day = 1;

	if (++m_month > 12) {
		m_month = 1;

		++m_year;
	}
}

static int request_lfclk(void)
{
	int ret;

	struct onoff_manager *mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_LF);
	if (!mgr) {
		LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
		return -ENXIO;
	}

	static struct k_poll_signal sig;
	k_poll_signal_init(&sig);
	sys_notify_init_signal(&m_lfclk_cli.notify, &sig);

	ret = onoff_request(mgr, &m_lfclk_cli);
	if (ret < 0) {
		LOG_ERR("Call `onoff_request` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &sig),
	};
	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	ret = request_lfclk();
	if (ret) {
		LOG_ERR("Call `request_lfclk` failed: %d", ret);
		return ret;
	}

	nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
	config.prescaler = 4095;
	nrfx_err_t err = nrfx_rtc_init(&m_rtc, &config, rtc_handler);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_rtc_init` failed: %d", (int)err);
		return -EIO;
	}

	nrfx_rtc_tick_enable(&m_rtc, true);
	nrfx_rtc_enable(&m_rtc);

	IRQ_CONNECT(RTC2_IRQn, 0, nrfx_rtc_2_irq_handler, NULL, 0);
	irq_enable(RTC2_IRQn);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_RTC_INIT_PRIORITY);
