/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-current"

struct app_config g_app_config;

static struct app_config m_app_config_interim = {
#if defined(CONFIG_SHIELD_CTR_LTE) && !defined(CONFIG_SHIELD_CTR_LRW)
	.mode = APP_CONFIG_MODE_LTE,
#elif defined(CONFIG_SHIELD_CTR_LRW) && !defined(CONFIG_SHIELD_CTR_LTE)
	.mode = APP_CONFIG_MODE_LRW,
#else
	.mode = APP_CONFIG_MODE_NONE,
#endif

	.interval_report = 900,

#if defined(CONFIG_SHIELD_CTR_Z)
	.event_report_delay = 1,
	.event_report_rate = 30,
	.backup_report_connected = true,
	.backup_report_disconnected = true,
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)
	.channel_interval_sample = 60,
	.channel_interval_aggreg = 300,
#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	.w1_therm_interval_sample = 60,
	.w1_therm_interval_aggreg = 300,
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */
};

int app_config_get_interval_report(void)
{
	return m_app_config_interim.interval_report;
}

int app_config_set_interval_report(int value)
{
	if (value < APP_CONFIG_INTERVAL_REPORT_MIN || value > APP_CONFIG_INTERVAL_REPORT_MAX) {
		return -ERANGE;
	}

	m_app_config_interim.interval_report = value;

	return 0;
}

static void print_config_mode(const struct shell *shell)
{
	const char *mode;
	switch (m_app_config_interim.mode) {
	case APP_CONFIG_MODE_NONE:
		mode = "none";
		break;
	case APP_CONFIG_MODE_LTE:
		mode = "lte";
		break;
	case APP_CONFIG_MODE_LRW:
		mode = "lrw";
		break;
	default:
		mode = "(unknown)";
		break;
	}

	shell_print(shell, "app config mode %s", mode);
}

static void print_interval_report(const struct shell *shell)
{
	shell_print(shell, "app config interval-report %d", m_app_config_interim.interval_report);
}

#if defined(CONFIG_SHIELD_CTR_Z)

static void print_event_report_delay(const struct shell *shell)
{
	shell_print(shell, "app config event-report-delay %d",
		    m_app_config_interim.event_report_delay);
}

static void print_event_report_rate(const struct shell *shell)
{
	shell_print(shell, "app config event-report-rate %d",
		    m_app_config_interim.event_report_rate);
}

static void print_backup_report_connected(const struct shell *shell)
{
	shell_print(shell, "app config backup-report-connected %s",
		    m_app_config_interim.backup_report_connected ? "true" : "false");
}

static void print_backup_report_disconnected(const struct shell *shell)
{
	shell_print(shell, "app config backup-report-disconnected %s",
		    m_app_config_interim.backup_report_disconnected ? "true" : "false");
}

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)

static void print_channel_interval_sample(const struct shell *shell)
{
	shell_print(shell, "app config channel-interval-sample %d",
		    m_app_config_interim.channel_interval_sample);
}

static void print_channel_interval_aggreg(const struct shell *shell)
{
	shell_print(shell, "app config channel-interval-aggreg %d",
		    m_app_config_interim.channel_interval_aggreg);
}

static void print_channel_active(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, "app config channel-active %d %s", i + 1,
			    m_app_config_interim.channel_active[i] ? "true" : "false");
	}
}

static void print_channel_differential(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, "app config channel-differential %d %s", i + 1,
			    m_app_config_interim.channel_differential[i] ? "true" : "false");
	}
}

static void print_channel_calib_x0(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, "app config channel-calib-x0 %d %d", i + 1,
			    m_app_config_interim.channel_calib_x0[i]);
	}
}

static void print_channel_calib_y0(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, "app config channel-calib-y0 %d %d", i + 1,
			    m_app_config_interim.channel_calib_y0[i]);
	}
}

static void print_channel_calib_x1(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, "app config channel-calib-x1 %d %d", i + 1,
			    m_app_config_interim.channel_calib_x1[i]);
	}
}

static void print_channel_calib_y1(const struct shell *shell, int channel)
{
	int ch = channel;

	for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT); i++) {
		shell_print(shell, "app config channel-calib-y1 %d %d", i + 1,
			    m_app_config_interim.channel_calib_y1[i]);
	}
}

#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

static void print_w1_therm_interval_sample(const struct shell *shell)
{
	shell_print(shell, "app config w1-therm-interval-sample %d",
		    m_app_config_interim.w1_therm_interval_sample);
}

static void print_w1_therm_interval_aggreg(const struct shell *shell)
{
	shell_print(shell, "app config w1-therm-interval-aggreg %d",
		    m_app_config_interim.w1_therm_interval_aggreg);
}

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_config_mode(shell);

	print_interval_report(shell);

#if defined(CONFIG_SHIELD_CTR_Z)
	print_event_report_delay(shell);
	print_event_report_rate(shell);
	print_backup_report_connected(shell);
	print_backup_report_disconnected(shell);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)
	print_channel_interval_sample(shell);
	print_channel_interval_aggreg(shell);
	print_channel_active(shell, 0);
	print_channel_differential(shell, 0);
	print_channel_calib_x0(shell, 0);
	print_channel_calib_y0(shell, 0);
	print_channel_calib_x1(shell, 0);
	print_channel_calib_y1(shell, 0);
#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	print_w1_therm_interval_sample(shell);
	print_w1_therm_interval_aggreg(shell);
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

	return 0;
}

int app_config_cmd_config_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_config_mode(shell);
		return 0;
	}

	if (argc == 2) {
		if (!strcmp("none", argv[1])) {
			m_app_config_interim.mode = APP_CONFIG_MODE_NONE;
			return 0;
		}

		if (!strcmp("lte", argv[1])) {
			m_app_config_interim.mode = APP_CONFIG_MODE_LTE;
			return 0;
		}

		if (!strcmp("lrw", argv[1])) {
			m_app_config_interim.mode = APP_CONFIG_MODE_LRW;
			return 0;
		}

		shell_error(shell, "invalid option");

		return -EINVAL;
	}

	shell_help(shell);

	return -EINVAL;
}

#define CMD_CONFIG_FUNCTION_INT(NAME, MIN, MAX)                                                    \
	int app_config_cmd_config_##NAME(const struct shell *shell, size_t argc, char **argv)      \
	{                                                                                          \
		if (argc == 1) {                                                                   \
			print_##NAME(shell);                                                       \
			return 0;                                                                  \
		}                                                                                  \
		if (argc == 2) {                                                                   \
			size_t len = strlen(argv[1]);                                              \
			for (size_t i = 0; i < len; i++) {                                         \
				if (!isdigit((int)argv[1][i])) {                                   \
					shell_error(shell, "invalid format");                      \
					return -EINVAL;                                            \
				}                                                                  \
			}                                                                          \
			long value = strtol(argv[1], NULL, 10);                                    \
			if (value < MIN || value > MAX) {                                          \
				shell_error(shell, "invalid range");                               \
				return -EINVAL;                                                    \
			}                                                                          \
			m_app_config_interim.NAME = (int)value;                                    \
			return 0;                                                                  \
		}                                                                                  \
		shell_help(shell);                                                                 \
		return -EINVAL;                                                                    \
	}

#define CMD_CONFIG_FUNCTION_BOOL(NAME)                                                             \
	int app_config_cmd_config_##NAME(const struct shell *shell, size_t argc, char **argv)      \
	{                                                                                          \
		if (argc == 1) {                                                                   \
			print_##NAME(shell);                                                       \
			return 0;                                                                  \
		}                                                                                  \
		if (argc == 2) {                                                                   \
			bool is_false = !strcmp(argv[1], "false");                                 \
			bool is_true = !strcmp(argv[1], "true");                                   \
			if (is_false) {                                                            \
				m_app_config_interim.NAME = false;                                 \
			} else if (is_true) {                                                      \
				m_app_config_interim.NAME = true;                                  \
			} else {                                                                   \
				shell_error(shell, "invalid format");                              \
				return -EINVAL;                                                    \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
		shell_help(shell);                                                                 \
		return -EINVAL;                                                                    \
	}

CMD_CONFIG_FUNCTION_INT(interval_report, 30, 86400);

#if defined(CONFIG_SHIELD_CTR_Z)

CMD_CONFIG_FUNCTION_INT(event_report_delay, 1, 86400);
CMD_CONFIG_FUNCTION_INT(event_report_rate, 1, 3600);

CMD_CONFIG_FUNCTION_BOOL(backup_report_connected);
CMD_CONFIG_FUNCTION_BOOL(backup_report_disconnected);

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)

CMD_CONFIG_FUNCTION_INT(channel_interval_sample, 1, 86400);
CMD_CONFIG_FUNCTION_INT(channel_interval_aggreg, 1, 86400);

int app_config_cmd_config_channel_active(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_active(shell, channel);
		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "true") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_active[i] = true;
		}

		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "false") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_active[i] = false;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_differential(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_differential(shell, channel);
		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "true") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_differential[i] = true;
		}

		return 0;
	}

	if (argc == 3 && strcmp(argv[2], "false") == 0) {
		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_differential[i] = false;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_x0(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_x0(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_x0[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_y0(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_y0(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_y0[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_x1(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_x1(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_x1[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_calib_y1(const struct shell *shell, size_t argc, char **argv)
{
	int channel;

	if (argc >= 2) {
		unsigned long ch = strtoul(argv[1], NULL, 10);

		if (ch < 0 || ch > APP_CONFIG_CHANNEL_COUNT) {
			shell_error(shell, "invalid channel index");
			return -EINVAL;
		}

		channel = ch;
	}

	if (argc == 2) {
		print_channel_calib_y1(shell, channel);
		return 0;
	}

	if (argc == 3) {
		long long val = strtoll(argv[2], NULL, 10);
		if (val < INT_MIN || val > INT_MAX) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		int ch = channel;

		for (int i = ch != 0 ? ch - 1 : 0; i < (ch != 0 ? ch : APP_CONFIG_CHANNEL_COUNT);
		     i++) {
			m_app_config_interim.channel_calib_y1[i] = val;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

CMD_CONFIG_FUNCTION_INT(w1_therm_interval_sample, 1, 86400);
CMD_CONFIG_FUNCTION_INT(w1_therm_interval_aggreg, 1, 86400);

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#undef CMD_CONFIG_FUNCTION_INT
#undef CMD_CONFIG_FUNCTION_BOOL

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET_ARRAY(_key, _var, _size)                                                      \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != _size) {                                                        \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, _var, len);                                          \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

#define SETTINGS_SET_SCALAR(_key, _var)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != sizeof(m_app_config_interim._var)) {                            \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, &m_app_config_interim._var, len);                    \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET_SCALAR("mode", mode);

	SETTINGS_SET_SCALAR("interval-report", interval_report);

#if defined(CONFIG_SHIELD_CTR_Z)
	SETTINGS_SET_SCALAR("event-report-delay", event_report_delay);
	SETTINGS_SET_SCALAR("event-report-rate", event_report_rate);
	SETTINGS_SET_SCALAR("backup-report-connected", backup_report_connected);
	SETTINGS_SET_SCALAR("backup-report-disconnected", backup_report_disconnected);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)
	SETTINGS_SET_SCALAR("channel-interval-sample", channel_interval_sample);
	SETTINGS_SET_SCALAR("channel-interval-aggreg", channel_interval_aggreg);

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET_SCALAR("channel-1-active", channel_active[0]);
	SETTINGS_SET_SCALAR("channel-2-active", channel_active[1]);
	SETTINGS_SET_SCALAR("channel-3-active", channel_active[2]);
	SETTINGS_SET_SCALAR("channel-4-active", channel_active[3]);

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET_SCALAR("channel-1-differential", channel_differential[0]);
	SETTINGS_SET_SCALAR("channel-2-differential", channel_differential[1]);
	SETTINGS_SET_SCALAR("channel-3-differential", channel_differential[2]);
	SETTINGS_SET_SCALAR("channel-4-differential", channel_differential[3]);

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET_SCALAR("channel-1-calib-x0", channel_calib_x0[0]);
	SETTINGS_SET_SCALAR("channel-2-calib-x0", channel_calib_x0[1]);
	SETTINGS_SET_SCALAR("channel-3-calib-x0", channel_calib_x0[2]);
	SETTINGS_SET_SCALAR("channel-4-calib-x0", channel_calib_x0[3]);

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET_SCALAR("channel-1-calib-y0", channel_calib_y0[0]);
	SETTINGS_SET_SCALAR("channel-2-calib-y0", channel_calib_y0[1]);
	SETTINGS_SET_SCALAR("channel-3-calib-y0", channel_calib_y0[2]);
	SETTINGS_SET_SCALAR("channel-4-calib-y0", channel_calib_y0[3]);

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET_SCALAR("channel-1-calib-x1", channel_calib_x1[0]);
	SETTINGS_SET_SCALAR("channel-2-calib-x1", channel_calib_x1[1]);
	SETTINGS_SET_SCALAR("channel-3-calib-x1", channel_calib_x1[2]);
	SETTINGS_SET_SCALAR("channel-4-calib-x1", channel_calib_x1[3]);

	/* TODO Replace with for-each pre-processor macro */
	SETTINGS_SET_SCALAR("channel-1-calib-y1", channel_calib_y1[0]);
	SETTINGS_SET_SCALAR("channel-2-calib-y1", channel_calib_y1[1]);
	SETTINGS_SET_SCALAR("channel-3-calib-y1", channel_calib_y1[2]);
	SETTINGS_SET_SCALAR("channel-4-calib-y1", channel_calib_y1[3]);
#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	SETTINGS_SET_SCALAR("w1-therm-interval-sample", w1_therm_interval_sample);
	SETTINGS_SET_SCALAR("w1-therm-interval-aggreg", w1_therm_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#undef SETTINGS_SET_ARRAY
#undef SETTINGS_SET_SCALAR

	return -ENOENT;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&g_app_config, &m_app_config_interim, sizeof(g_app_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
#define EXPORT_FUNC_ARRAY(_key, _var, _size)                                                       \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, _size);                             \
	} while (0)

#define EXPORT_FUNC_SCALAR(_key, _var)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, &m_app_config_interim._var,               \
				  sizeof(m_app_config_interim._var));                              \
	} while (0)

	EXPORT_FUNC_SCALAR("mode", mode);

	EXPORT_FUNC_SCALAR("interval-report", interval_report);

#if defined(CONFIG_SHIELD_CTR_Z)
	EXPORT_FUNC_SCALAR("event-report-delay", event_report_delay);
	EXPORT_FUNC_SCALAR("event-report-rate", event_report_rate);
	EXPORT_FUNC_SCALAR("backup-report-connected", backup_report_connected);
	EXPORT_FUNC_SCALAR("backup-report-disconnected", backup_report_disconnected);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_K1)

	EXPORT_FUNC_SCALAR("channel-interval-sample", channel_interval_sample);
	EXPORT_FUNC_SCALAR("channel-interval-aggreg", channel_interval_aggreg);

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC_SCALAR("channel-1-active", channel_active[0]);
	EXPORT_FUNC_SCALAR("channel-2-active", channel_active[1]);
	EXPORT_FUNC_SCALAR("channel-3-active", channel_active[2]);
	EXPORT_FUNC_SCALAR("channel-4-active", channel_active[3]);

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC_SCALAR("channel-1-differential", channel_differential[0]);
	EXPORT_FUNC_SCALAR("channel-2-differential", channel_differential[1]);
	EXPORT_FUNC_SCALAR("channel-3-differential", channel_differential[2]);
	EXPORT_FUNC_SCALAR("channel-4-differential", channel_differential[3]);

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC_SCALAR("channel-1-calib-x0", channel_calib_x0[0]);
	EXPORT_FUNC_SCALAR("channel-2-calib-x0", channel_calib_x0[1]);
	EXPORT_FUNC_SCALAR("channel-3-calib-x0", channel_calib_x0[2]);
	EXPORT_FUNC_SCALAR("channel-4-calib-x0", channel_calib_x0[3]);

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC_SCALAR("channel-1-calib-y0", channel_calib_y0[0]);
	EXPORT_FUNC_SCALAR("channel-2-calib-y0", channel_calib_y0[1]);
	EXPORT_FUNC_SCALAR("channel-3-calib-y0", channel_calib_y0[2]);
	EXPORT_FUNC_SCALAR("channel-4-calib-y0", channel_calib_y0[3]);

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC_SCALAR("channel-1-calib-x1", channel_calib_x1[0]);
	EXPORT_FUNC_SCALAR("channel-2-calib-x1", channel_calib_x1[1]);
	EXPORT_FUNC_SCALAR("channel-3-calib-x1", channel_calib_x1[2]);
	EXPORT_FUNC_SCALAR("channel-4-calib-x1", channel_calib_x1[3]);

	/* TODO Replace with for-each pre-processor macro */
	EXPORT_FUNC_SCALAR("channel-1-calib-y1", channel_calib_y1[0]);
	EXPORT_FUNC_SCALAR("channel-2-calib-y1", channel_calib_y1[1]);
	EXPORT_FUNC_SCALAR("channel-3-calib-y1", channel_calib_y1[2]);
	EXPORT_FUNC_SCALAR("channel-4-calib-y1", channel_calib_y1[3]);
#endif /* defined(CONFIG_SHIELD_CTR_K1) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	EXPORT_FUNC_SCALAR("w1-therm-interval-sample", w1_therm_interval_sample);
	EXPORT_FUNC_SCALAR("w1-therm-interval-aggreg", w1_therm_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#undef EXPORT_FUNC_ARRAY
#undef EXPORT_FUNC_SCALAR

	return 0;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);
	if (ret) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);
	if (ret) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, app_config_cmd_config_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
