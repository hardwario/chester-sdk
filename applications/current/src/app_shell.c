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
#include <chester/drivers/ctr_k1.h>

#include <math.h>
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

#if defined(CONFIG_SHIELD_CTR_K1)

static const struct device *m_ctr_k1_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_k1));

static int read_channel_raw(int channel, float *avg, float *rms)
{
	if (!device_is_ready(m_ctr_k1_dev)) {
		return -ENODEV;
	}

	int ch_idx = channel - 1;
	enum ctr_k1_channel channels[1];

	if (g_app_config.channel_differential[ch_idx]) {
		channels[0] = CTR_K1_CHANNEL_DIFFERENTIAL(channel);
	} else {
		channels[0] = CTR_K1_CHANNEL_SINGLE_ENDED(channel);
	}

	struct ctr_k1_result results[1];

	/* Power on */
	int ret = ctr_k1_set_power(m_ctr_k1_dev, channels[0], true);
	if (ret) {
		return ret;
	}

	k_sleep(K_MSEC(100));

	/* Measure */
	ret = ctr_k1_measure(m_ctr_k1_dev, channels, 1, results);

	/* Power off */
	ctr_k1_set_power(m_ctr_k1_dev, channels[0], false);

	if (ret) {
		return ret;
	}

	*avg = results[0].avg;
	*rms = results[0].rms;
	return 0;
}

#define DEFINE_CHANNEL_COMMANDS(n)                                                                 \
	static int cmd_channel_##n##_read(const struct shell *shell, size_t argc, char **argv)     \
	{                                                                                          \
		float avg, rms;                                                                    \
		int ret = read_channel_raw(n, &avg, &rms);                                         \
		if (ret) {                                                                         \
			shell_error(shell, "Read failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		/* Select raw value based on mode */                                               \
		int mode = APP_CONFIG_GET_CALIB_MODE(g_app_config, n - 1);                         \
		float raw = mode ? rms : avg;                                                      \
		/* Calculate calibrated value */                                                   \
		float x0 = g_app_config.channel_calib_x0[n - 1];                                   \
		float y0 = g_app_config.channel_calib_y0[n - 1];                                   \
		float x1 = g_app_config.channel_calib_x1[n - 1];                                   \
		float y1 = g_app_config.channel_calib_y1[n - 1];                                   \
		if (x1 != x0) {                                                                    \
			float calibrated = (y0 * (x1 - raw) + y1 * (raw - x0)) / (float)(x1 - x0); \
			shell_print(shell,                                                         \
				    "Channel %d: avg=%.1f rms=%.1f mV (mode=%s, "                  \
				    "calibrated: %.2f)",                                           \
				    n, (double)avg, (double)rms, mode ? "rms" : "avg",             \
				    (double)calibrated);                                           \
		} else {                                                                           \
			shell_print(shell,                                                         \
				    "Channel %d: avg=%.1f rms=%.1f mV (mode=%s, "                  \
				    "no calibration)",                                             \
				    n, (double)avg, (double)rms, mode ? "rms" : "avg");            \
		}                                                                                  \
		return 0;                                                                          \
	}                                                                                          \
                                                                                                   \
	static int cmd_channel_##n##_calib_0(const struct shell *shell, size_t argc, char **argv)  \
	{                                                                                          \
		if (argc < 2) {                                                                    \
			shell_error(shell, "Usage: set-0 <real_value>");                           \
			return -EINVAL;                                                            \
		}                                                                                  \
		float y0 = strtof(argv[1], NULL);                                                  \
		float avg, rms;                                                                    \
		int ret = read_channel_raw(n, &avg, &rms);                                         \
		if (ret) {                                                                         \
			shell_error(shell, "Read failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		int mode = APP_CONFIG_GET_CALIB_MODE(g_app_config, n - 1);                         \
		float raw = mode ? rms : avg;                                                      \
		g_app_config.channel_calib_x0[n - 1] = raw;                                        \
		g_app_config.channel_calib_y0[n - 1] = y0;                                         \
		ret = app_config_save_channel_calib(n);                                            \
		if (ret) {                                                                         \
			shell_error(shell, "Save failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		shell_print(shell,                                                                 \
			    "Channel %d: avg=%.1f rms=%.1f (using %s), point 0 set "               \
			    "(x0=%.2f, y0=%.2f)",                                                  \
			    n, (double)avg, (double)rms, mode ? "rms" : "avg", (double)raw,        \
			    (double)y0);                                                           \
		return 0;                                                                          \
	}                                                                                          \
                                                                                                   \
	static int cmd_channel_##n##_calib_1(const struct shell *shell, size_t argc, char **argv)  \
	{                                                                                          \
		if (argc < 2) {                                                                    \
			shell_error(shell, "Usage: set-1 <real_value>");                           \
			return -EINVAL;                                                            \
		}                                                                                  \
		float y1 = strtof(argv[1], NULL);                                                  \
		float avg, rms;                                                                    \
		int ret = read_channel_raw(n, &avg, &rms);                                         \
		if (ret) {                                                                         \
			shell_error(shell, "Read failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		int mode = APP_CONFIG_GET_CALIB_MODE(g_app_config, n - 1);                         \
		float raw = mode ? rms : avg;                                                      \
		g_app_config.channel_calib_x1[n - 1] = raw;                                        \
		g_app_config.channel_calib_y1[n - 1] = y1;                                         \
		ret = app_config_save_channel_calib(n);                                            \
		if (ret) {                                                                         \
			shell_error(shell, "Save failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		shell_print(shell,                                                                 \
			    "Channel %d: avg=%.1f rms=%.1f (using %s), point 1 set "               \
			    "(x1=%.2f, y1=%.2f)",                                                  \
			    n, (double)avg, (double)rms, mode ? "rms" : "avg", (double)raw,        \
			    (double)y1);                                                           \
		return 0;                                                                          \
	}                                                                                          \
                                                                                                   \
	static int cmd_channel_##n##_calib_show(const struct shell *shell, size_t argc,            \
						char **argv)                                       \
	{                                                                                          \
		int mode = APP_CONFIG_GET_CALIB_MODE(g_app_config, n - 1);                         \
		shell_print(shell,                                                                 \
			    "Channel %d calibration: x0=%.2f y0=%.2f, x1=%.2f y1=%.2f, "           \
			    "mode=%s",                                                             \
			    n, (double)g_app_config.channel_calib_x0[n - 1],                       \
			    (double)g_app_config.channel_calib_y0[n - 1],                          \
			    (double)g_app_config.channel_calib_x1[n - 1],                          \
			    (double)g_app_config.channel_calib_y1[n - 1], mode ? "rms" : "avg");   \
		return 0;                                                                          \
	}                                                                                          \
                                                                                                   \
	static int cmd_channel_##n##_calib_reset(const struct shell *shell, size_t argc,           \
						 char **argv)                                      \
	{                                                                                          \
		g_app_config.channel_calib_x0[n - 1] = 0;                                          \
		g_app_config.channel_calib_y0[n - 1] = 0;                                          \
		g_app_config.channel_calib_x1[n - 1] = 0;                                          \
		g_app_config.channel_calib_y1[n - 1] = 0;                                          \
		int ret = app_config_save_channel_calib(n);                                        \
		if (ret) {                                                                         \
			shell_error(shell, "Save failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		shell_print(shell, "Channel %d calibration reset", n);                             \
		return 0;                                                                          \
	}                                                                                          \
                                                                                                   \
	static int cmd_channel_##n##_calib_mode(const struct shell *shell, size_t argc,            \
						char **argv)                                       \
	{                                                                                          \
		if (argc < 2) {                                                                    \
			int mode = APP_CONFIG_GET_CALIB_MODE(g_app_config, n - 1);                 \
			shell_print(shell, "Channel %d mode: %s", n, mode ? "rms" : "avg");        \
			return 0;                                                                  \
		}                                                                                  \
		if (strcmp(argv[1], "avg") == 0) {                                                 \
			APP_CONFIG_SET_CALIB_MODE(g_app_config, n - 1, 0);                         \
		} else if (strcmp(argv[1], "rms") == 0) {                                          \
			APP_CONFIG_SET_CALIB_MODE(g_app_config, n - 1, 1);                         \
		} else {                                                                           \
			shell_error(shell, "Unknown mode: %s (use avg or rms)", argv[1]);          \
			return -EINVAL;                                                            \
		}                                                                                  \
		int ret = app_config_save_channel_calib(n);                                        \
		if (ret) {                                                                         \
			shell_error(shell, "Save failed: %d", ret);                                \
			return ret;                                                                \
		}                                                                                  \
		shell_print(shell, "Channel %d mode set to: %s", n, argv[1]);                      \
		return 0;                                                                          \
	}

DEFINE_CHANNEL_COMMANDS(1)
DEFINE_CHANNEL_COMMANDS(2)
DEFINE_CHANNEL_COMMANDS(3)
DEFINE_CHANNEL_COMMANDS(4)

#endif /* CONFIG_SHIELD_CTR_K1 */

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

#if defined(CONFIG_SHIELD_CTR_K1)

/* Manual subcmd definitions to allow hyphens */
#define DEFINE_CHANNEL_SUBCMDS(n)                                                              \
	static const struct shell_static_entry shell_sub_channel_##n##_calib[] = {             \
		{                                                                              \
			.syntax = "set-0",                                                     \
			.help = "Set calibration point 0 <real_value>",                        \
			.subcmd = NULL,                                                        \
			.handler = cmd_channel_##n##_calib_0,                                  \
			.args = { .mandatory = 2, .optional = 0 }                              \
		},                                                                             \
		{                                                                              \
			.syntax = "set-1",                                                     \
			.help = "Set calibration point 1 <real_value>",                        \
			.subcmd = NULL,                                                        \
			.handler = cmd_channel_##n##_calib_1,                                  \
			.args = { .mandatory = 2, .optional = 0 }                              \
		},                                                                             \
		{                                                                              \
			.syntax = "show",                                                      \
			.help = "Show calibration",                                            \
			.subcmd = NULL,                                                        \
			.handler = cmd_channel_##n##_calib_show,                               \
			.args = { .mandatory = 1, .optional = 0 }                              \
		},                                                                             \
		{                                                                              \
			.syntax = "reset",                                                     \
			.help = "Reset calibration to 0",                                      \
			.subcmd = NULL,                                                        \
			.handler = cmd_channel_##n##_calib_reset,                              \
			.args = { .mandatory = 1, .optional = 0 }                              \
		},                                                                             \
		{                                                                              \
			.syntax = "mode",                                                      \
			.help = "Set/show mode (avg|rms)",                                     \
			.subcmd = NULL,                                                        \
			.handler = cmd_channel_##n##_calib_mode,                               \
			.args = { .mandatory = 1, .optional = 1 }                              \
		},                                                                             \
		SHELL_SUBCMD_SET_END                                                           \
	};                                                                                     \
	static const union shell_cmd_entry sub_channel_##n##_calib = {                         \
		.entry = shell_sub_channel_##n##_calib                                         \
	};                                                                                     \
                                                                                               \
	static const struct shell_static_entry shell_sub_channel_##n[] = {                     \
		{                                                                              \
			.syntax = "read",                                                      \
			.help = "Read raw mV value",                                           \
			.subcmd = NULL,                                                        \
			.handler = cmd_channel_##n##_read,                                     \
			.args = { .mandatory = 1, .optional = 0 }                              \
		},                                                                             \
		{                                                                              \
			.syntax = "calib",                                                     \
			.help = "Calibration commands",                                        \
			.subcmd = &sub_channel_##n##_calib,                                    \
			.handler = print_help,                                                 \
			.args = { .mandatory = 1, .optional = 0 }                              \
		},                                                                             \
		SHELL_SUBCMD_SET_END                                                           \
	};                                                                                     \
	static const union shell_cmd_entry sub_channel_##n = {                                 \
		.entry = shell_sub_channel_##n                                                 \
	}

DEFINE_CHANNEL_SUBCMDS(1);
DEFINE_CHANNEL_SUBCMDS(2);
DEFINE_CHANNEL_SUBCMDS(3);
DEFINE_CHANNEL_SUBCMDS(4);

/* clang-format off */

/* Manual definition to allow hyphens in command names */
static const struct shell_static_entry shell_sub_current[] = {
	{
		.syntax = "channel-1",
		.help = "Channel 1 commands",
		.subcmd = &sub_channel_1,
		.handler = print_help,
		.args = { .mandatory = 1, .optional = 0 }
	},
	{
		.syntax = "channel-2",
		.help = "Channel 2 commands",
		.subcmd = &sub_channel_2,
		.handler = print_help,
		.args = { .mandatory = 1, .optional = 0 }
	},
	{
		.syntax = "channel-3",
		.help = "Channel 3 commands",
		.subcmd = &sub_channel_3,
		.handler = print_help,
		.args = { .mandatory = 1, .optional = 0 }
	},
	{
		.syntax = "channel-4",
		.help = "Channel 4 commands",
		.subcmd = &sub_channel_4,
		.handler = print_help,
		.args = { .mandatory = 1, .optional = 0 }
	},
	SHELL_SUBCMD_SET_END
};

static const union shell_cmd_entry sub_current = {
	.entry = shell_sub_current
};

SHELL_CMD_REGISTER(current, &sub_current, "Current application commands.", print_help);

/* clang-format on */

#endif /* CONFIG_SHIELD_CTR_K1 */

/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
