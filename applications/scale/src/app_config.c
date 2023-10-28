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

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app"

struct app_config g_app_config;
static struct app_config m_app_config_interim = {
	.channel_a1_active = true,
	.channel_a2_active = true,
	.channel_b1_active = true,
	.channel_b2_active = true,
	.weight_measurement_interval = 60,

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	.people_measurement_interval = 600,
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	.report_interval = 900,

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	.people_counter_power_off_delay = 30,
	.people_counter_stay_timeout = 5,
	.people_counter_adult_border = 4,
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */
};

static void print_channel_a1_active(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config channel-a1-active %s",
		    m_app_config_interim.channel_a1_active ? "true" : "false");
}

static void print_channel_a2_active(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config channel-a2-active %s",
		    m_app_config_interim.channel_a2_active ? "true" : "false");
}

static void print_channel_b1_active(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config channel-b1-active %s",
		    m_app_config_interim.channel_b1_active ? "true" : "false");
}

static void print_channel_b2_active(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config channel-b2-active %s",
		    m_app_config_interim.channel_b2_active ? "true" : "false");
}

static void print_weight_measurement_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config weight-measurement-interval %d",
		    m_app_config_interim.weight_measurement_interval);
}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)

static void print_people_measurement_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config people-measurement-interval %d",
		    m_app_config_interim.people_measurement_interval);
}

#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

static void print_report_interval(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config report-interval %d",
		    m_app_config_interim.report_interval);
}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)

static void print_people_counter_power_off_delay(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config people-counter-power-off-delay %d",
		    m_app_config_interim.people_counter_power_off_delay);
}

static void print_people_counter_stay_timeout(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config people-counter-stay-timeout %d",
		    m_app_config_interim.people_counter_stay_timeout);
}

static void print_people_counter_adult_border(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config people-counter-adult-border %d",
		    m_app_config_interim.people_counter_adult_border);
}

#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_channel_a1_active(shell);
	print_channel_a2_active(shell);
	print_channel_b1_active(shell);
	print_channel_b2_active(shell);
	print_weight_measurement_interval(shell);

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	print_people_measurement_interval(shell);
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	print_report_interval(shell);

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	print_people_counter_power_off_delay(shell);
	print_people_counter_stay_timeout(shell);
	print_people_counter_adult_border(shell);
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	return 0;
}

int app_config_cmd_config_channel_a1_active(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_channel_a1_active(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_app_config_interim.channel_a1_active = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_app_config_interim.channel_a1_active = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_a2_active(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_channel_a2_active(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_app_config_interim.channel_a2_active = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_app_config_interim.channel_a2_active = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_b1_active(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_channel_b1_active(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_app_config_interim.channel_b1_active = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_app_config_interim.channel_b1_active = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_channel_b2_active(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_channel_b2_active(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_app_config_interim.channel_b2_active = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_app_config_interim.channel_b2_active = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_weight_measurement_interval(const struct shell *shell, size_t argc,
						      char **argv)
{
	if (argc == 1) {
		print_weight_measurement_interval(shell);
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

		int weight_measurement_interval = atoi(argv[1]);

		if (weight_measurement_interval < 5 || weight_measurement_interval > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.weight_measurement_interval = weight_measurement_interval;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)

int app_config_cmd_config_people_measurement_interval(const struct shell *shell, size_t argc,
						      char **argv)
{
	if (argc == 1) {
		print_people_measurement_interval(shell);
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

		int people_measurement_interval = atoi(argv[1]);

		if (people_measurement_interval < 5 || people_measurement_interval > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.people_measurement_interval = people_measurement_interval;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

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

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)

int app_config_cmd_config_people_counter_power_off_delay(const struct shell *shell, size_t argc,
							 char **argv)
{
	if (argc == 1) {
		print_people_counter_power_off_delay(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 3) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int people_counter_power_off_delay = atoi(argv[1]);

		if (people_counter_power_off_delay < 0 || people_counter_power_off_delay > 255) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.people_counter_power_off_delay =
			people_counter_power_off_delay;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_people_counter_stay_timeout(const struct shell *shell, size_t argc,
						      char **argv)
{
	if (argc == 1) {
		print_people_counter_stay_timeout(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 3) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int people_counter_stay_timeout = atoi(argv[1]);

		if (people_counter_stay_timeout < 0 || people_counter_stay_timeout > 255) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.people_counter_stay_timeout = people_counter_stay_timeout;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_people_counter_adult_border(const struct shell *shell, size_t argc,
						      char **argv)
{
	if (argc == 1) {
		print_people_counter_adult_border(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 1) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int people_counter_adult_border = atoi(argv[1]);

		if (people_counter_adult_border < 0 || people_counter_adult_border > 8) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.people_counter_adult_border = people_counter_adult_border;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

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

	SETTINGS_SET("channel-a1-active", &m_app_config_interim.channel_a1_active,
		     sizeof(m_app_config_interim.channel_a1_active));
	SETTINGS_SET("channel-a2-active", &m_app_config_interim.channel_a2_active,
		     sizeof(m_app_config_interim.channel_a2_active));
	SETTINGS_SET("channel-b1-active", &m_app_config_interim.channel_b1_active,
		     sizeof(m_app_config_interim.channel_b1_active));
	SETTINGS_SET("channel-b2-active", &m_app_config_interim.channel_b2_active,
		     sizeof(m_app_config_interim.channel_b2_active));
	SETTINGS_SET("weight-measurement-interval",
		     &m_app_config_interim.weight_measurement_interval,
		     sizeof(m_app_config_interim.weight_measurement_interval));

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	SETTINGS_SET("people-measurement-interval",
		     &m_app_config_interim.people_measurement_interval,
		     sizeof(m_app_config_interim.people_measurement_interval));
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	SETTINGS_SET("report-interval", &m_app_config_interim.report_interval,
		     sizeof(m_app_config_interim.report_interval));

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	SETTINGS_SET("people-counter-power-off-delay",
		     &m_app_config_interim.people_counter_power_off_delay,
		     sizeof(m_app_config_interim.people_counter_power_off_delay));
	SETTINGS_SET("people-counter-stay-timeout",
		     &m_app_config_interim.people_counter_stay_timeout,
		     sizeof(m_app_config_interim.people_counter_stay_timeout));
	SETTINGS_SET("people-counter-adult-border",
		     &m_app_config_interim.people_counter_adult_border,
		     sizeof(m_app_config_interim.people_counter_adult_border));
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

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

	EXPORT_FUNC("channel-a1-active", &m_app_config_interim.channel_a1_active,
		    sizeof(m_app_config_interim.channel_a1_active));
	EXPORT_FUNC("channel-a2-active", &m_app_config_interim.channel_a2_active,
		    sizeof(m_app_config_interim.channel_a2_active));
	EXPORT_FUNC("channel-b1-active", &m_app_config_interim.channel_b1_active,
		    sizeof(m_app_config_interim.channel_b1_active));
	EXPORT_FUNC("channel-b2-active", &m_app_config_interim.channel_b2_active,
		    sizeof(m_app_config_interim.channel_b2_active));
	EXPORT_FUNC("weight-measurement-interval",
		    &m_app_config_interim.weight_measurement_interval,
		    sizeof(m_app_config_interim.weight_measurement_interval));

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	EXPORT_FUNC("people-measurement-interval",
		    &m_app_config_interim.people_measurement_interval,
		    sizeof(m_app_config_interim.people_measurement_interval));
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	EXPORT_FUNC("report-interval", &m_app_config_interim.report_interval,
		    sizeof(m_app_config_interim.report_interval));

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	EXPORT_FUNC("people-counter-power-off-delay",
		    &m_app_config_interim.people_counter_power_off_delay,
		    sizeof(m_app_config_interim.people_counter_power_off_delay));
	EXPORT_FUNC("people-counter-stay-timeout",
		    &m_app_config_interim.people_counter_stay_timeout,
		    sizeof(m_app_config_interim.people_counter_stay_timeout));
	EXPORT_FUNC("people-counter-adult-border",
		    &m_app_config_interim.people_counter_adult_border,
		    sizeof(m_app_config_interim.people_counter_adult_border));
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

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
