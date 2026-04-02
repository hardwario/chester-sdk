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
#include "app_data.h"
#include "app_sensor.h"
#include "feature.h"

#include <chester/ctr_adc.h>

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

static int cmd_poll(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	app_work_poll();

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

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

static const char *mode_names[] = {"disabled", "trigger", "counter", "voltage", "current"};

static int x0_adc_lookup[] = {
	CTR_ADC_CHANNEL_A0, CTR_ADC_CHANNEL_A1, CTR_ADC_CHANNEL_A2, CTR_ADC_CHANNEL_A3,
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	CTR_ADC_CHANNEL_B0, CTR_ADC_CHANNEL_B1, CTR_ADC_CHANNEL_B2, CTR_ADC_CHANNEL_B3,
#endif
};

static float channel_read_adc(int ch_idx)
{
	uint16_t sample;
	int ret = ctr_adc_read(x0_adc_lookup[ch_idx], &sample);

	if (ret) {
		return NAN;
	}

	if (g_app_data.voltage[ch_idx]) {
		return (float)CTR_ADC_MILLIVOLTS(sample) * (10.f + 1.f) / 1000.f;
	} else if (g_app_data.current[ch_idx]) {
		return CTR_ADC_X0_CL_MILLIAMPS(sample);
	}

	return NAN;
}

static int get_channel_mode(int ch_idx)
{
	switch (ch_idx) {
	case 0:
		return (int)g_app_config.channel_mode_1;
	case 1:
		return (int)g_app_config.channel_mode_2;
	case 2:
		return (int)g_app_config.channel_mode_3;
	case 3:
		return (int)g_app_config.channel_mode_4;
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	case 4:
		return (int)g_app_config.channel_mode_5;
	case 5:
		return (int)g_app_config.channel_mode_6;
	case 6:
		return (int)g_app_config.channel_mode_7;
	case 7:
		return (int)g_app_config.channel_mode_8;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_B) */
	default:
		return -1;
	}
}

static void channel_print(const struct shell *shell, int ch_idx)
{
	int ch_num = ch_idx + 1;
	int mode = get_channel_mode(ch_idx);

	if (mode < 0 || mode > 4) {
		shell_print(shell, "channel %d: invalid mode", ch_num);
		return;
	}

	if (mode == APP_CONFIG_CHANNEL_MODE_DISABLED) {
		shell_print(shell, "channel %d: disabled", ch_num);
		return;
	}

	app_data_lock();

	if (g_app_data.trigger[ch_idx]) {
		struct app_data_trigger *t = g_app_data.trigger[ch_idx];

		shell_print(shell, "channel %d: trigger, state: %s, events: %d", ch_num,
			    t->trigger_active ? "active" : "inactive", t->event_count);
	} else if (g_app_data.counter[ch_idx]) {
		struct app_data_counter *c = g_app_data.counter[ch_idx];

		shell_print(shell, "channel %d: counter, value: %llu, delta: %llu", ch_num,
			    c->value, c->delta);
	} else if (g_app_data.voltage[ch_idx]) {
		struct app_data_analog *v = g_app_data.voltage[ch_idx];

		if (isnan(v->last_sample)) {
			shell_print(shell,
				    "channel %d: voltage, last: NaN, samples: %d, "
				    "measurements: %d",
				    ch_num, v->sample_count, v->measurement_count);
		} else {
			shell_print(shell,
				    "channel %d: voltage, last: %.2f V, samples: %d, "
				    "measurements: %d",
				    ch_num, (double)v->last_sample, v->sample_count,
				    v->measurement_count);
		}
	} else if (g_app_data.current[ch_idx]) {
		struct app_data_analog *c = g_app_data.current[ch_idx];

		if (isnan(c->last_sample)) {
			shell_print(shell,
				    "channel %d: current, last: NaN, samples: %d, "
				    "measurements: %d",
				    ch_num, c->sample_count, c->measurement_count);
		} else {
			shell_print(shell,
				    "channel %d: current, last: %.2f mA, samples: %d, "
				    "measurements: %d",
				    ch_num, (double)c->last_sample, c->sample_count,
				    c->measurement_count);
		}
	} else {
		shell_print(shell, "channel %d: %s (not detected)", ch_num, mode_names[mode]);
	}

	app_data_unlock();
}

static int cmd_channel_show(const struct shell *shell, size_t argc, char **argv)
{
	app_sensor_voltage_sample();
	app_sensor_current_sample();

	if (argc <= 1) {
		for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {
			channel_print(shell, i);
		}
	} else {
		for (int a = 1; a < argc; a++) {
			int ch_num = atoi(argv[a]);

			if (ch_num < 1 || ch_num > APP_DATA_NUM_CHANNELS) {
				shell_error(shell, "invalid channel: %s (1-%d)", argv[a],
					    APP_DATA_NUM_CHANNELS);
				return -EINVAL;
			}
			channel_print(shell, ch_num - 1);
		}
	}

	return 0;
}

static int cmd_channel_read(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "usage: channel read <1-%d> [duration_s]",
			    APP_DATA_NUM_CHANNELS);
		return -EINVAL;
	}

	int ch_num = atoi(argv[1]);

	if (ch_num < 1 || ch_num > APP_DATA_NUM_CHANNELS) {
		shell_error(shell, "invalid channel: %s (1-%d)", argv[1], APP_DATA_NUM_CHANNELS);
		return -EINVAL;
	}

	int duration = 10;

	if (argc > 2) {
		duration = atoi(argv[2]);
		if (duration < 1) {
			duration = 1;
		}
	}

	int ch_idx = ch_num - 1;

	for (int i = 0; i < duration; i++) {
		if (g_app_data.voltage[ch_idx] || g_app_data.current[ch_idx]) {
			float val = channel_read_adc(ch_idx);
			if (g_app_data.voltage[ch_idx]) {
				shell_print(shell, "channel %d: voltage, live: %.2f V", ch_num,
					    (double)val);
			} else {
				shell_print(shell, "channel %d: current, live: %.2f mA", ch_num,
					    (double)val);
			}
		} else {
			channel_print(shell, ch_idx);
		}
		k_sleep(K_SECONDS(1));
	}

	shell_print(shell, "channel read finished");

	return 0;
}

static void channel_reset(const struct shell *shell, int ch_idx)
{
	int ch_num = ch_idx + 1;

	app_data_lock();

	if (g_app_data.trigger[ch_idx]) {
		g_app_data.trigger[ch_idx]->event_count = 0;
		shell_print(shell, "channel %d: trigger reset", ch_num);
	} else if (g_app_data.counter[ch_idx]) {
		g_app_data.counter[ch_idx]->value = 0;
		g_app_data.counter[ch_idx]->last_value = 0;
		g_app_data.counter[ch_idx]->delta = 0;
		g_app_data.counter[ch_idx]->measurement_count = 0;
		shell_print(shell, "channel %d: counter reset", ch_num);
	} else if (g_app_data.voltage[ch_idx]) {
		g_app_data.voltage[ch_idx]->sample_count = 0;
		g_app_data.voltage[ch_idx]->measurement_count = 0;
		g_app_data.voltage[ch_idx]->last_sample = NAN;
		shell_print(shell, "channel %d: voltage reset", ch_num);
	} else if (g_app_data.current[ch_idx]) {
		g_app_data.current[ch_idx]->sample_count = 0;
		g_app_data.current[ch_idx]->measurement_count = 0;
		g_app_data.current[ch_idx]->last_sample = NAN;
		shell_print(shell, "channel %d: current reset", ch_num);
	} else {
		shell_print(shell, "channel %d: not active", ch_num);
	}

	app_data_unlock();
}

static int cmd_channel_reset(const struct shell *shell, size_t argc, char **argv)
{
	if (argc <= 1) {
		for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {
			channel_reset(shell, i);
		}
	} else {
		int ch_num = atoi(argv[1]);

		if (ch_num < 1 || ch_num > APP_DATA_NUM_CHANNELS) {
			shell_error(shell, "invalid channel: %s (1-%d)", argv[1],
				    APP_DATA_NUM_CHANNELS);
			return -EINVAL;
		}
		channel_reset(shell, ch_num - 1);
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

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
SHELL_CMD_REGISTER(poll, NULL, "Poll cloud immediately.", cmd_poll);

/* ### Preserved code "functions 2" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
SHELL_STATIC_SUBCMD_SET_CREATE(sub_channel,
			       SHELL_CMD_ARG(show, NULL, "Show channel status.",
					     cmd_channel_show, 1, 8),
			       SHELL_CMD_ARG(read, NULL, "Read channel: read <channel> [timeout_s]",
					     cmd_channel_read, 2, 1),
			       SHELL_CMD_ARG(reset, NULL, "Reset channel data: reset [channel]",
					     cmd_channel_reset, 1, 1),
			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(channel, &sub_channel, "Channel commands.", print_help);
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
