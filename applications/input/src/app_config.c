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
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-input"

struct app_config g_app_config;

static struct app_config m_app_config_interim = {
	.interval_report = 1800,

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
	.event_report_delay = 5,
	.event_report_rate = 30,
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
	.backup_report_connected = true,
	.backup_report_disconnected = true,
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	.trigger_input_type = APP_CONFIG_INPUT_TYPE_NPN,
	.trigger_duration_active = 100,
	.trigger_duration_inactive = 100,
	.trigger_cooldown_time = 10,

	.counter_interval_aggreg = 300,
	.counter_input_type = APP_CONFIG_INPUT_TYPE_NPN,
	.counter_duration_active = 2,
	.counter_duration_inactive = 2,
	.counter_cooldown_time = 10,
	.analog_interval_sample = 60,
	.analog_interval_aggreg = 300,
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)
	.hygro_interval_sample = 60,
	.hygro_interval_aggreg = 300,
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	.w1_therm_interval_sample = 60,
	.w1_therm_interval_aggreg = 300,
#endif /* defined(COINFG_SHIELD_CTR_DS18B20)*/
};

static void print_interval_report(const struct shell *shell)
{
	shell_print(shell, "app config interval-report %d", m_app_config_interim.interval_report);
}

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)

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

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)

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

#if defined(CONFIG_SHIELD_CTR_X0_A)

static void print_trigger_input_type(const struct shell *shell)
{
	const char *type;

	switch (m_app_config_interim.trigger_input_type) {
	case APP_CONFIG_INPUT_TYPE_NPN:
		type = "npn";
		break;
	case APP_CONFIG_INPUT_TYPE_PNP:
		type = "pnp";
		break;
	default:
		type = "(unknown)";
		break;
	}

	shell_print(shell, "app config trigger-input-type %s", type);
}

static void print_trigger_duration_active(const struct shell *shell)
{
	shell_print(shell, "app config trigger-duration-active %d",
		    m_app_config_interim.trigger_duration_active);
}

static void print_trigger_duration_inactive(const struct shell *shell)
{
	shell_print(shell, "app config trigger-duration-inactive %d",
		    m_app_config_interim.trigger_duration_inactive);
}

static void print_trigger_cooldown_time(const struct shell *shell)
{
	shell_print(shell, "app config trigger-cooldown-time %d",
		    m_app_config_interim.trigger_cooldown_time);
}

static void print_trigger_report_active(const struct shell *shell)
{
	shell_print(shell, "app config trigger-report-active %s",
		    m_app_config_interim.trigger_report_active ? "true" : "false");
}

static void print_trigger_report_inactive(const struct shell *shell)
{
	shell_print(shell, "app config trigger-report-inactive %s",
		    m_app_config_interim.trigger_report_inactive ? "true" : "false");
}

static void print_counter_interval_aggreg(const struct shell *shell)
{
	shell_print(shell, "app config counter-interval-aggreg %d",
		    m_app_config_interim.counter_interval_aggreg);
}

static void print_counter_input_type(const struct shell *shell)
{
	const char *type;

	switch (m_app_config_interim.counter_input_type) {
	case APP_CONFIG_INPUT_TYPE_NPN:
		type = "npn";
		break;
	case APP_CONFIG_INPUT_TYPE_PNP:
		type = "pnp";
		break;
	default:
		type = "(unknown)";
		break;
	}

	shell_print(shell, "app config counter-input-type %s", type);
}

static void print_counter_duration_active(const struct shell *shell)
{
	shell_print(shell, "app config counter-duration-active %d",
		    m_app_config_interim.counter_duration_active);
}

static void print_counter_duration_inactive(const struct shell *shell)
{
	shell_print(shell, "app config counter-duration-inactive %d",
		    m_app_config_interim.counter_duration_inactive);
}

static void print_counter_cooldown_time(const struct shell *shell)
{
	shell_print(shell, "app config counter-cooldown-time %d",
		    m_app_config_interim.counter_cooldown_time);
}

static void print_analog_interval_sample(const struct shell *shell)
{
	shell_print(shell, "app config analog-interval-sample %d",
		    m_app_config_interim.analog_interval_sample);
}

static void print_analog_interval_aggreg(const struct shell *shell)
{
	shell_print(shell, "app config analog-interval-aggreg %d",
		    m_app_config_interim.analog_interval_aggreg);
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)

static void print_hygro_interval_sample(const struct shell *shell)
{
	shell_print(shell, "app config hygro-interval-sample %d",
		    m_app_config_interim.hygro_interval_sample);
}

static void print_hygro_interval_aggreg(const struct shell *shell)
{
	shell_print(shell, "app config hygro-interval-aggreg %d",
		    m_app_config_interim.hygro_interval_aggreg);
}

#endif /* defined(CONFIG_SHIELD_CTR_S2) */

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
	print_interval_report(shell);

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
	print_event_report_delay(shell);
	print_event_report_rate(shell);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
	print_backup_report_connected(shell);
	print_backup_report_disconnected(shell);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	print_trigger_input_type(shell);
	print_trigger_duration_active(shell);
	print_trigger_duration_inactive(shell);
	print_trigger_cooldown_time(shell);
	print_trigger_report_active(shell);
	print_trigger_report_inactive(shell);

	print_counter_interval_aggreg(shell);
	print_counter_input_type(shell);
	print_counter_duration_active(shell);
	print_counter_duration_inactive(shell);
	print_counter_cooldown_time(shell);
	print_analog_interval_sample(shell);
	print_analog_interval_aggreg(shell);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)
	print_hygro_interval_sample(shell);
	print_hygro_interval_aggreg(shell);
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	print_w1_therm_interval_sample(shell);
	print_w1_therm_interval_aggreg(shell);
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

	return 0;
}

int app_config_cmd_config_interval_report(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_interval_report(shell);
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

		long interval_report = strtol(argv[1], NULL, 10);

		if (interval_report < 30 || interval_report > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.interval_report = (int)interval_report;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)

int app_config_cmd_config_event_report_delay(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_event_report_delay(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.event_report_delay = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_event_report_rate(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_event_report_rate(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.event_report_rate = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)

int app_config_cmd_config_backup_report_connected(const struct shell *shell, size_t argc,
						  char **argv)
{
	if (argc == 1) {
		print_backup_report_connected(shell);
		return 0;
	}

	if (argc == 2) {
		bool is_false = !strcmp(argv[1], "false");
		bool is_true = !strcmp(argv[1], "true");

		if (is_false) {
			m_app_config_interim.backup_report_connected = false;
		} else if (is_true) {
			m_app_config_interim.backup_report_connected = true;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_backup_report_disconnected(const struct shell *shell, size_t argc,
						     char **argv)
{
	if (argc == 1) {
		print_backup_report_disconnected(shell);
		return 0;
	}

	if (argc == 2) {
		bool is_false = !strcmp(argv[1], "false");
		bool is_true = !strcmp(argv[1], "true");

		if (is_false) {
			m_app_config_interim.backup_report_disconnected = false;
		} else if (is_true) {
			m_app_config_interim.backup_report_disconnected = true;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)

int app_config_cmd_config_trigger_input_type(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_trigger_input_type(shell);
		return 0;
	}

	if (argc == 2) {
		bool is_npn = !strcmp(argv[1], "npn");
		bool is_pnp = !strcmp(argv[1], "pnp");

		if (is_npn) {
			m_app_config_interim.trigger_input_type = APP_CONFIG_INPUT_TYPE_NPN;
		} else if (is_pnp) {
			m_app_config_interim.trigger_input_type = APP_CONFIG_INPUT_TYPE_PNP;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_trigger_duration_active(const struct shell *shell, size_t argc,
						  char **argv)
{
	if (argc == 1) {
		print_trigger_duration_active(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 0 || value > 60000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.trigger_duration_active = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_trigger_duration_inactive(const struct shell *shell, size_t argc,
						    char **argv)
{
	if (argc == 1) {
		print_trigger_duration_inactive(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 0 || value > 60000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.trigger_duration_inactive = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_trigger_cooldown_time(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_trigger_cooldown_time(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 0 || value > 60000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.trigger_cooldown_time = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_trigger_report_active(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_trigger_report_active(shell);
		return 0;
	}

	if (argc == 2) {
		bool is_false = !strcmp(argv[1], "false");
		bool is_true = !strcmp(argv[1], "true");

		if (is_false) {
			m_app_config_interim.trigger_report_active = false;
		} else if (is_true) {
			m_app_config_interim.trigger_report_active = true;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_trigger_report_inactive(const struct shell *shell, size_t argc,
						  char **argv)
{
	if (argc == 1) {
		print_trigger_report_inactive(shell);
		return 0;
	}

	if (argc == 2) {
		bool is_false = !strcmp(argv[1], "false");
		bool is_true = !strcmp(argv[1], "true");

		if (is_false) {
			m_app_config_interim.trigger_report_inactive = 0;
		} else if (is_true) {
			m_app_config_interim.trigger_report_inactive = 1;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_counter_interval_aggreg(const struct shell *shell, size_t argc,
						  char **argv)
{
	if (argc == 1) {
		print_counter_interval_aggreg(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.counter_interval_aggreg = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_counter_input_type(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_counter_input_type(shell);
		return 0;
	}

	if (argc == 2) {
		bool is_npn = !strcmp(argv[1], "npn");
		bool is_pnp = !strcmp(argv[1], "pnp");

		if (is_npn) {
			m_app_config_interim.counter_input_type = APP_CONFIG_INPUT_TYPE_NPN;
		} else if (is_pnp) {
			m_app_config_interim.counter_input_type = APP_CONFIG_INPUT_TYPE_PNP;
		} else {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_counter_duration_active(const struct shell *shell, size_t argc,
						  char **argv)
{
	if (argc == 1) {
		print_counter_duration_active(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 0 || value > 60000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.counter_duration_active = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_counter_duration_inactive(const struct shell *shell, size_t argc,
						    char **argv)
{
	if (argc == 1) {
		print_counter_duration_inactive(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 0 || value > 60000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.counter_duration_inactive = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_counter_cooldown_time(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_counter_cooldown_time(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 0 || value > 60000) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.counter_cooldown_time = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_analog_interval_sample(const struct shell *shell, size_t argc,
						 char **argv)
{
	if (argc == 1) {
		print_analog_interval_sample(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.analog_interval_sample = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_analog_interval_aggreg(const struct shell *shell, size_t argc,
						 char **argv)
{
	if (argc == 1) {
		print_analog_interval_aggreg(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.analog_interval_aggreg = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)

int app_config_cmd_config_hygro_interval_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_hygro_interval_sample(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.hygro_interval_sample = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_hygro_interval_aggreg(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_hygro_interval_aggreg(shell);
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

		long value = strtol(argv[1], NULL, 10);

		if (value < 1 || value > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.hygro_interval_aggreg = (int)value;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)

int app_config_cmd_config_w1_therm_interval_sample(const struct shell *shell, size_t argc,
						   char **argv)
{
	if (argc == 1) {
		print_w1_therm_interval_sample(shell);
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

		long interval_sample = strtol(argv[1], NULL, 10);

		if (interval_sample < 1 || interval_sample > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.w1_therm_interval_sample = (int)interval_sample;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int app_config_cmd_config_w1_therm_interval_aggreg(const struct shell *shell, size_t argc,
						   char **argv)
{
	if (argc == 1) {
		print_w1_therm_interval_aggreg(shell);
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

		long interval_aggreg = strtol(argv[1], NULL, 10);

		if (interval_aggreg < 1 || interval_aggreg > 86400) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_app_config_interim.w1_therm_interval_aggreg = (int)interval_aggreg;

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

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

	SETTINGS_SET_SCALAR("interval-report", interval_report);

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
	SETTINGS_SET_SCALAR("event-report-delay", event_report_delay);
	SETTINGS_SET_SCALAR("event-report-rate", event_report_rate);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
	SETTINGS_SET_SCALAR("backup-report-connected", backup_report_connected);
	SETTINGS_SET_SCALAR("backup-report-disconnected", backup_report_disconnected);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	SETTINGS_SET_SCALAR("trigger-input-type", trigger_input_type);
	SETTINGS_SET_SCALAR("trigger-duration-active", trigger_duration_active);
	SETTINGS_SET_SCALAR("trigger-duration-inactive", trigger_duration_inactive);
	SETTINGS_SET_SCALAR("trigger-cooldown-time", trigger_cooldown_time);
	SETTINGS_SET_SCALAR("trigger-report-active", trigger_report_active);
	SETTINGS_SET_SCALAR("trigger-report-inactive", trigger_report_inactive);

	SETTINGS_SET_SCALAR("counter-interval-aggreg", counter_interval_aggreg);
	SETTINGS_SET_SCALAR("counter-input-type", counter_input_type);
	SETTINGS_SET_SCALAR("counter-duration-active", counter_duration_active);
	SETTINGS_SET_SCALAR("counter-duration-inactive", counter_duration_inactive);
	SETTINGS_SET_SCALAR("counter-cooldown-time", counter_cooldown_time);
	SETTINGS_SET_SCALAR("analog-interval-sample", analog_interval_sample);
	SETTINGS_SET_SCALAR("analog-interval-aggreg", analog_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)
	SETTINGS_SET_SCALAR("hygro-interval-sample", hygro_interval_sample);
	SETTINGS_SET_SCALAR("hygro-interval-aggreg", hygro_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	SETTINGS_SET_SCALAR("w1-therm-interval-sample", w1_therm_interval_sample);
	SETTINGS_SET_SCALAR("w1-therm-interval-aggreg", w1_therm_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#undef SETTINGS_SET_ARRAY

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

	EXPORT_FUNC_SCALAR("interval-report", interval_report);

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z)
	EXPORT_FUNC_SCALAR("event-report-delay", event_report_delay);
	EXPORT_FUNC_SCALAR("event-report-rate", event_report_rate);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_Z)
	EXPORT_FUNC_SCALAR("backup-report-connected", backup_report_connected);
	EXPORT_FUNC_SCALAR("backup-report-disconnected", backup_report_disconnected);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	EXPORT_FUNC_SCALAR("trigger-input-type", trigger_input_type);
	EXPORT_FUNC_SCALAR("trigger-duration-active", trigger_duration_active);
	EXPORT_FUNC_SCALAR("trigger-duration-inactive", trigger_duration_inactive);
	EXPORT_FUNC_SCALAR("trigger-cooldown-time", trigger_cooldown_time);
	EXPORT_FUNC_SCALAR("trigger-report-active", trigger_report_active);
	EXPORT_FUNC_SCALAR("trigger-report-inactive", trigger_report_inactive);

	EXPORT_FUNC_SCALAR("counter-interval-aggreg", counter_interval_aggreg);
	EXPORT_FUNC_SCALAR("counter-input-type", counter_input_type);
	EXPORT_FUNC_SCALAR("counter-duration-active", counter_duration_active);
	EXPORT_FUNC_SCALAR("counter-duration-inactive", counter_duration_inactive);
	EXPORT_FUNC_SCALAR("counter-cooldown-time", counter_cooldown_time);
	EXPORT_FUNC_SCALAR("analog-interval-sample", analog_interval_sample);
	EXPORT_FUNC_SCALAR("analog-interval-aggreg", analog_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_S2)
	EXPORT_FUNC_SCALAR("hygro-interval-sample", hygro_interval_sample);
	EXPORT_FUNC_SCALAR("hygro-interval-aggreg", hygro_interval_aggreg);
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

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
