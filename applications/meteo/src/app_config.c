/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

/* CHESTER includes */
#include <chester/ctr_config.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-clime"

struct app_config g_app_config;

static struct app_config m_app_config_interim = {
	.interval_sample = 60,
	.interval_aggreg = 300,
	.interval_report = 1800,

#if defined(CONFIG_SHIELD_CTR_Z)
	.event_report_delay = 1,
	.event_report_rate = 30,
	.backup_report_connected = true,
	.backup_report_disconnected = true,
#endif /* defined(CONFIG_SHIELD_CTR_Z) */
};

static void print_interval_sample(const struct shell *shell)
{
	shell_print(shell, "app config interval-sample %d", m_app_config_interim.interval_sample);
}

static void print_interval_aggreg(const struct shell *shell)
{
	shell_print(shell, "app config interval-aggreg %d", m_app_config_interim.interval_aggreg);
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

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_interval_sample(shell);
	print_interval_aggreg(shell);
	print_interval_report(shell);

#if defined(CONFIG_SHIELD_CTR_Z)
	print_event_report_delay(shell);
	print_event_report_rate(shell);
	print_backup_report_connected(shell);
	print_backup_report_disconnected(shell);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

	return 0;
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

CMD_CONFIG_FUNCTION_INT(interval_sample, 1, 86400);
CMD_CONFIG_FUNCTION_INT(interval_aggreg, 1, 86400);
CMD_CONFIG_FUNCTION_INT(interval_report, 30, 86400);

#if defined(CONFIG_SHIELD_CTR_Z)

CMD_CONFIG_FUNCTION_INT(event_report_delay, 1, 86400);
CMD_CONFIG_FUNCTION_INT(event_report_rate, 1, 3600);

CMD_CONFIG_FUNCTION_BOOL(backup_report_connected);
CMD_CONFIG_FUNCTION_BOOL(backup_report_disconnected);

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

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
                                                                                                   \
			ret = read_cb(cb_arg, _var, len);                                          \
                                                                                                   \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
                                                                                                   \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

#define SETTINGS_SET_SCALAR(_key, _var)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != sizeof(m_app_config_interim._var)) {                            \
				return -EINVAL;                                                    \
			}                                                                          \
                                                                                                   \
			ret = read_cb(cb_arg, &m_app_config_interim._var, len);                    \
                                                                                                   \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
                                                                                                   \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET_SCALAR("interval-sample", interval_sample);
	SETTINGS_SET_SCALAR("interval-aggreg", interval_aggreg);
	SETTINGS_SET_SCALAR("interval-report", interval_report);

#if defined(CONFIG_SHIELD_CTR_Z)
	SETTINGS_SET_SCALAR("event-report-delay", event_report_delay);
	SETTINGS_SET_SCALAR("event-report-rate", event_report_rate);
	SETTINGS_SET_SCALAR("backup-report-connected", backup_report_connected);
	SETTINGS_SET_SCALAR("backup-report-disconnected", backup_report_disconnected);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

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

	EXPORT_FUNC_SCALAR("interval-sample", interval_sample);
	EXPORT_FUNC_SCALAR("interval-aggreg", interval_aggreg);
	EXPORT_FUNC_SCALAR("interval-report", interval_report);

#if defined(CONFIG_SHIELD_CTR_Z)
	EXPORT_FUNC_SCALAR("event-report-delay", event_report_delay);
	EXPORT_FUNC_SCALAR("event-report-rate", event_report_rate);
	EXPORT_FUNC_SCALAR("backup-report-connected", backup_report_connected);
	EXPORT_FUNC_SCALAR("backup-report-disconnected", backup_report_disconnected);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

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
