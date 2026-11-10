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
#include "app_handler.h"
#include "app_sensor.h"
#include "app_sensor_drv.h"
#include "app_x0_map.h"
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

/* Upper bound for the `sensor read`/`channel read` loops, which sleep a second
 * per iteration on the shell thread. */
#define APP_SENSOR_READ_MAX_COUNT 60

static const char *mode_names[] = {"disabled", "trigger", "counter",
				   "voltage",  "current", "power"};

static float channel_read_adc(int ch_idx)
{
	uint16_t sample;
	int ret = ctr_adc_read(app_x0_adc_map[ch_idx], &sample);

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

static void channel_print(const struct shell *shell, int ch_idx)
{
	int ch_num = ch_idx + 1;
	int mode = (int)app_config_get_channel_mode(ch_idx);

	/* A configured sensor owns its power/measure channels. Ownership is derived
	 * at init, not written back to config, so channel-mode-N may still read as
	 * whatever the user set — report the sensor role instead, which is both more
	 * specific and what the hardware actually does. */
	int owner = app_config_channel_owner(ch_num);
	if (owner >= 0) {
		const char *role = (g_app_config.sensors[owner].ch_power == ch_num) ? "power"
										    : "measure";
		shell_print(shell, "channel %d: owned by sensor-%d (%s)", ch_num, owner + 1, role);
		return;
	}

	if (mode < 0 || mode >= (int)ARRAY_SIZE(mode_names)) {
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
		} else if (duration > APP_SENSOR_READ_MAX_COUNT) {
			shell_warn(shell, "duration capped at %d s", APP_SENSOR_READ_MAX_COUNT);
			duration = APP_SENSOR_READ_MAX_COUNT;
		}
	}

	int ch_idx = ch_num - 1;

	/* A sensor's measure channel has a voltage record allocated so the pair
	 * describes itself in the payload, which would send us down the live-ADC branch
	 * below. That reading would be meaningless — the channel is parked high-Z with
	 * the excitation off — and it is taken without the sensor layer's lock, so it
	 * can also land in the middle of a real sample. `channel_print` already refuses
	 * owned channels; do the same here and point at the command that does drive the
	 * hardware properly. */
	int owner = app_config_channel_owner(ch_num);

	if (owner >= 0) {
		shell_error(shell,
			    "channel %d is driven by sensor-%d; use `sensor read %d` — reading it "
			    "here would sample an unexcited pin",
			    ch_num, owner + 1, owner + 1);
		return -EBUSY;
	}

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

static int cmd_sensor_show(const struct shell *shell, size_t argc, char **argv)
{
	int any = 0;

	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		struct app_data_sensor *slot = &g_app_data.sensors[i];

		if (!slot->configured) {
			continue;
		}

		const struct app_sensor_driver *drv =
			app_sensor_find_driver_by_type(slot->type_id);

		if (drv && drv->print_data) {
			drv->print_data(shell, i);
		} else {
			shell_print(shell, "[%d] type %u: no driver", i + 1, slot->type_id);
		}
		if (!slot->ready) {
			shell_warn(shell, "[%d] setup failed; retried before each sample", i + 1);
		}
		any = 1;
	}

	if (!any) {
		shell_print(shell, "no sensors configured");
	}

	return 0;
}

static int cmd_sensor_read(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "usage: sensor read <1-%d> [count]", APP_SENSORS_MAX);
		return -EINVAL;
	}

	int n = atoi(argv[1]);

	if (n < 1 || n > APP_SENSORS_MAX) {
		shell_error(shell, "invalid sensor: %s (1-%d)", argv[1], APP_SENSORS_MAX);
		return -EINVAL;
	}

	int slot = n - 1;

	if (!g_app_data.sensors[slot].configured) {
		shell_error(shell, "sensor-%d not configured", n);
		return -EINVAL;
	}

	/* Each iteration sleeps a second on the shell thread, so cap the count --
	 * an unbounded value would wedge the shell for hours. */
	int count = 1;

	if (argc > 2) {
		count = atoi(argv[2]);
		if (count < 1) {
			count = 1;
		} else if (count > APP_SENSOR_READ_MAX_COUNT) {
			shell_warn(shell, "count capped at %d", APP_SENSOR_READ_MAX_COUNT);
			count = APP_SENSOR_READ_MAX_COUNT;
		}
	}

	const struct app_sensor_driver *drv =
		app_sensor_find_driver_by_type(g_app_data.sensors[slot].type_id);

	for (int i = 0; i < count; i++) {
		int ret = app_sensor_drv_sample_slot(slot);

		/* A positive return is APP_SENSOR_SAMPLE_ALARM, not a failure. The read
		 * has already consumed the state change, so the next periodic sample will
		 * not see it again: request the report here, or the alarm would wait for
		 * the next scheduled uplink. */
		if (ret < 0) {
			shell_error(shell, "sample failed: %d", ret);
			return ret;
		}
		if (ret == APP_SENSOR_SAMPLE_ALARM) {
			shell_print(shell, "state changed; requesting a report");
			app_handler_report_event();
		}
		if (drv && drv->print_data) {
			drv->print_data(shell, slot);
		}
		if (i + 1 < count) {
			k_sleep(K_SECONDS(1));
		}
	}

	return 0;
}

static int cmd_sensor_scan(const struct shell *shell, size_t argc, char **argv)
{
	int slot = -1; /* -1 means "resolve the slot from the supply channel". */

	if (argc > 2) {
		slot = atoi(argv[2]);
		if (slot < 1 || slot > APP_SENSORS_MAX) {
			shell_error(shell, "invalid sensor slot: %s (1-%d)", argv[2],
				    APP_SENSORS_MAX);
			return -EINVAL;
		}
	}

	/* -1 means "use the configured sensor's supply channel". */
	return app_sensor_drv_scan(shell, argc > 1 ? atoi(argv[1]) : -1, slot);
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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_sensor,
			       SHELL_CMD_ARG(show, NULL, "Show sensor data.",
					     cmd_sensor_show, 1, 0),
			       SHELL_CMD_ARG(read, NULL, "Live read: read <sensor> [count]",
					     cmd_sensor_read, 2, 1),
			       SHELL_CMD_ARG(scan, NULL,
					     "Find which channel the sensor is wired to: "
					     "scan [supply_channel] [slot]",
					     cmd_sensor_scan, 1, 2),
			       SHELL_SUBCMD_SET_END);
SHELL_CMD_REGISTER(sensor, &sub_sensor, "Sensor commands.", print_help);
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
/* ^^^ Preserved code "functions 2" (end) */

/* clang-format on */
