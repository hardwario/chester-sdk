/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_work.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdlib.h>

/* ### Preserved code "includes" (begin) */
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_INF);

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_work_sample();

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_work_send();

	return 0;
}

static int cmd_aggreg(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_work_aggreg();

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

/* ### Preserved code "functions 1" (begin) */

static int cmd_read(const struct shell *shell, size_t argc, char **argv)
{
	__unused int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

#if defined(CONFIG_MPL3115A2_SHELL)
	shell_print(shell, "Barometer");
	ret = shell_execute_cmd(shell, "mpl3115a2 read");
	if (ret) {
		LOG_ERR("Call `shell_execute_cmd` failed: %d", ret);
	}
#endif /* defined(CONFIG_MPL3115A2_SHELL) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A)
	shell_print(shell, "Meteo");
	ret = shell_execute_cmd(shell, "meteo read ctr_meteo_a");
	if (ret) {
		LOG_ERR("Call `shell_execute_cmd` failed: %d", ret);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	shell_print(shell, "Meteo");
	ret = shell_execute_cmd(shell, "meteo read ctr_meteo_b");
	if (ret) {
		LOG_ERR("Call `shell_execute_cmd` failed: %d", ret);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_B) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	shell_print(shell, "Humidity");
	ret = shell_execute_cmd(shell, "hygro read");
	if (ret) {
		LOG_ERR("Call `shell_execute_cmd` failed: %d", ret);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

	return 0;
}

/* ^^^ Preserved code "functions 1" (end) */

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app,

			       SHELL_CMD_ARG(config, NULL, "Configurations commands.",
					     app_config_cmd_config, 1, 3),

/* ### Preserved code "subcmd" (begin) */
/* ^^^ Preserved code "subcmd" (end) */

			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(app, &sub_app, "Application commands.", print_help);

SHELL_CMD_REGISTER(sample, NULL, "Sample immediately.", cmd_sample);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
SHELL_CMD_REGISTER(aggreg, NULL, "Aggregate data immediately", cmd_aggreg);

/* ### Preserved code "functions 2" (begin) */
SHELL_CMD_REGISTER(sensors_read, NULL, "Read sensors and print values to shell", cmd_read);
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
