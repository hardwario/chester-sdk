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
#include <stdlib.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app"

struct app_config g_app_config;
static struct app_config m_app_config_interim = {
	.active_filter = 50,
	.inactive_filter = 50,
	.report_interval = 300,
};

static void print_active_filter(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config active-filter %d",
		    m_app_config_interim.active_filter);
}

static void print_inactive_filter(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config inactive-filter %d",
		    m_app_config_interim.inactive_filter);
}

static void print_report_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config report-interval %d",
		    m_app_config_interim.report_interval);
}

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_active_filter(shell);
	print_inactive_filter(shell);
	print_report_interval(shell);

	return 0;
}

int app_config_cmd_config_active_filter(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_active_filter(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int active_filter = atoi(argv[1]);

		if (active_filter < 10 || active_filter > 3600000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.active_filter = active_filter;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_inactive_filter(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_inactive_filter(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int inactive_filter = atoi(argv[1]);

		if (inactive_filter < 10 || inactive_filter > 3600000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.inactive_filter = inactive_filter;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_report_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_report_interval(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int report_interval = atoi(argv[1]);

		if (report_interval < 30 || report_interval > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.report_interval = report_interval;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET(_key, _var, _size)                                                            \
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

	SETTINGS_SET("active-filter", &m_app_config_interim.active_filter,
		     sizeof(m_app_config_interim.active_filter));
	SETTINGS_SET("inactive-filter", &m_app_config_interim.inactive_filter,
		     sizeof(m_app_config_interim.inactive_filter));
	SETTINGS_SET("report-interval", &m_app_config_interim.report_interval,
		     sizeof(m_app_config_interim.report_interval));

#undef SETTINGS_SET

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
#define EXPORT_FUNC(_key, _var, _size)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, _size);                             \
	} while (0)

	EXPORT_FUNC("active-filter", &m_app_config_interim.active_filter,
		    sizeof(m_app_config_interim.active_filter));
	EXPORT_FUNC("inactive-filter", &m_app_config_interim.inactive_filter,
		    sizeof(m_app_config_interim.inactive_filter));
	EXPORT_FUNC("report-interval", &m_app_config_interim.report_interval,
		    sizeof(m_app_config_interim.report_interval));

#undef EXPORT_FUNC

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

	if (ret < 0) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);

	if (ret < 0) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, app_config_cmd_config_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
