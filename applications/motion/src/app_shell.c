/*
 * Copyright (c) 2025 HARDWARIO a.s.
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
#include "app_handler.h"
#include "app_sensor.h"
#include <chester/ctr_rtc.h>
#include <time.h>

#if defined(CONFIG_CTR_S3)
void app_shell_sample_with_print(const struct shell *shell);
#endif
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_shell, LOG_LEVEL_INF);

static int cmd_sample(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_shell_sample_with_print(shell);

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
#if defined(CONFIG_CTR_S3)
static const struct shell *m_sample_shell;

static void sample_cb(void *user_data)
{
	if (m_sample_shell) {
		app_sensor_print_last_sample(m_sample_shell);
		m_sample_shell = NULL;
	}
}

void app_shell_sample_with_print(const struct shell *shell)
{
	m_sample_shell = shell;
	app_work_sample_with_cb(sample_cb, NULL);
}
#endif

#if defined(CONFIG_CTR_S3)
static void detection_cb(const struct app_detection_event *event, void *user_data)
{
	const struct shell *shell = (const struct shell *)user_data;
	uint64_t timestamp;
	ctr_rtc_get_ts(&timestamp);

	time_t ts = (time_t)timestamp;
	struct tm *tm_info = gmtime(&ts);
	char time_buf[12];
	strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

	const char *dir_str = (event->direction == 1) ? "L->R" : "R->L";

	shell_print(shell, "[%s] %s | delta=%dms | L=%d R=%d | motion L->R=%d R->L=%d", time_buf,
		    dir_str, event->delta_ms, event->detect_left, event->detect_right,
		    event->motion_right, event->motion_left);
}

static int cmd_motion_detection(const struct shell *shell, size_t argc, char **argv)
{
	int timeout = 60;

	if (argc > 1) {
		timeout = atoi(argv[1]);
		if (timeout < 1 || timeout > 1800) {
			shell_error(shell, "timeout must be 1-1800 seconds");
			return -EINVAL;
		}
	}

	shell_print(shell, "Monitoring motion detection for %d seconds...", timeout);
	shell_print(shell, "Format: [time] direction | delta | L R | pass L->R R->L");
	shell_print(shell, "");

	app_handler_set_detection_cb(detection_cb, (void *)shell);

	k_sleep(K_SECONDS(timeout));

	app_handler_clear_detection_cb();
	shell_print(shell, "");
	shell_print(shell, "Detection monitoring stopped.");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_app_motion,
			       SHELL_CMD_ARG(detection, NULL,
					     "Monitor motion detection [timeout_s].",
					     cmd_motion_detection, 1, 1),
			       SHELL_CMD_ARG(samples, NULL, "Show motion samples.", cmd_motion_view,
					     1, 0),
			       SHELL_SUBCMD_SET_END);
#endif
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

/* ### Preserved code "functions 2" (begin) */
#if defined(CONFIG_CTR_S3)
SHELL_CMD_REGISTER(motion, &sub_app_motion, "Motion commands.", print_help);
#endif
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
