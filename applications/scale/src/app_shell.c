/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_measure.h"
#include "app_send.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_INF);

static int cmd_measure(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&g_app_measure_weight_timer, K_NO_WAIT, K_FOREVER);

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	k_timer_start(&g_app_measure_people_timer, K_NO_WAIT, K_FOREVER);
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	k_timer_start(&g_app_send_timer, K_NO_WAIT, K_FOREVER);

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
	sub_app_config,

	SHELL_CMD_ARG(show, NULL,
	              "List current configuration.",
	              app_config_cmd_config_show, 1, 0),

	SHELL_CMD_ARG(channel-a1-active, NULL,
	              "Get/Set channel A1 activation (format: <true|false>).",
	              app_config_cmd_config_channel_a1_active, 1, 1),

	SHELL_CMD_ARG(channel-a2-active, NULL,
	              "Get/Set channel A2 activation (format: <true|false>).",
	              app_config_cmd_config_channel_a2_active, 1, 1),

	SHELL_CMD_ARG(channel-b1-active, NULL,
	              "Get/Set channel B1 activation (format: <true|false>).",
	              app_config_cmd_config_channel_b1_active, 1, 1),

	SHELL_CMD_ARG(channel-b2-active, NULL,
	              "Get/Set channel B2 activation (format: <true|false>).",
	              app_config_cmd_config_channel_b2_active, 1, 1),

	SHELL_CMD_ARG(weight-measurement-interval, NULL,
	              "Get/Set weight measurement interval in seconds (format: <5-3600>).",
	              app_config_cmd_config_weight_measurement_interval, 1, 1),

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	SHELL_CMD_ARG(people-measurement-interval, NULL,
	              "Get/Set people measurement interval in seconds (format: <5-3600>).",
	              app_config_cmd_config_people_measurement_interval, 1, 1),
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	SHELL_CMD_ARG(report-interval, NULL,
	              "Get/Set report interval in seconds (format: <30-86400>).",
	              app_config_cmd_config_report_interval, 1, 1),

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	SHELL_CMD_ARG(people-counter-power-off-delay, NULL,
	              "Get/Set People Counter power off delay in seconds (format: <0-255>).",
	              app_config_cmd_config_people_counter_power_off_delay, 1, 1),

	SHELL_CMD_ARG(people-counter-stay-timeout, NULL,
	              "Get/Set People Counter stay timeout in seconds (format: <0-255>).",
	              app_config_cmd_config_people_counter_stay_timeout, 1, 1),

	SHELL_CMD_ARG(people-counter-adult-border, NULL,
	              "Get/Set People Counter adult border (format: <0-8>).",
	              app_config_cmd_config_people_counter_adult_border, 1, 1),
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_app,

	SHELL_CMD_ARG(config, &sub_app_config, "Configuration commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);
SHELL_CMD_REGISTER(measure, NULL, "Start measurement immediately.", cmd_measure);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);

/* clang-format on */
