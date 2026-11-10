/*
 * Copyright (c) 2024 HARDWARIO a.s.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ### Preserved code "includes" (begin) */
#include "app_data.h"
#include "app_sensor_drv.h"
#include "drivers/drv_ld81.h"

#include <errno.h>

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
static int sensor_parse_cb(const struct shell *shell, char *argv,
			   const struct ctr_config_item *item);
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
/* ^^^ Preserved code "includes" (end) */

LOG_MODULE_REGISTER(app_config, LOG_LEVEL_DBG);

#define SETTINGS_PFX "app-control"

struct app_config g_app_config;

static struct app_config m_config_interim;

/* clang-format off */
const struct ctr_config_item items[] = {
	CTR_CONFIG_ITEM_INT("interval-report", m_config_interim.interval_report, 30, 86400, "Get/Set report interval in seconds.", 1800),
	CTR_CONFIG_ITEM_INT("interval-poll", m_config_interim.interval_poll, 0, 86400, "Get/Set poll interval in seconds.", 0),
	CTR_CONFIG_ITEM_INT("downlink-wdg-interval", m_config_interim.downlink_wdg_interval, 0, 1209600, "Get/Set downlink watchdog interval in seconds (disabled if 0).", 129600),
	CTR_CONFIG_ITEM_INT("event-report-delay", m_config_interim.event_report_delay, 1, 86400, "Get/Set event report delay in seconds.", 5),
	CTR_CONFIG_ITEM_INT("event-report-rate", m_config_interim.event_report_rate, 1, 3600, "Get/Set event report rate in reports per hour.", 30),

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	CTR_CONFIG_ITEM_ENUM("channel-mode-1", m_config_interim.channel_mode_1, ((const char*[]){"disabled", "trigger", "counter", "voltage", "current", "power"}), "Get/Set channel-mode-1", APP_CONFIG_CHANNEL_MODE_DISABLED),
	CTR_CONFIG_ITEM_ENUM("channel-mode-2", m_config_interim.channel_mode_2, ((const char*[]){"disabled", "trigger", "counter", "voltage", "current", "power"}), "Get/Set channel-mode-2", APP_CONFIG_CHANNEL_MODE_DISABLED),
	CTR_CONFIG_ITEM_ENUM("channel-mode-3", m_config_interim.channel_mode_3, ((const char*[]){"disabled", "trigger", "counter", "voltage", "current", "power"}), "Get/Set channel-mode-3", APP_CONFIG_CHANNEL_MODE_DISABLED),
	CTR_CONFIG_ITEM_ENUM("channel-mode-4", m_config_interim.channel_mode_4, ((const char*[]){"disabled", "trigger", "counter", "voltage", "current", "power"}), "Get/Set channel-mode-4", APP_CONFIG_CHANNEL_MODE_DISABLED),
	CTR_CONFIG_ITEM_ENUM("trigger-input-type", m_config_interim.trigger_input_type, ((const char*[]){"npn", "pnp"}), "Get/Set trigger input type", APP_CONFIG_INPUT_TYPE_NPN),
	CTR_CONFIG_ITEM_ENUM("counter-input-type", m_config_interim.counter_input_type, ((const char*[]){"npn", "pnp"}), "Get/Set counter input type", APP_CONFIG_INPUT_TYPE_NPN),
	CTR_CONFIG_ITEM_INT("trigger-duration-active", m_config_interim.trigger_duration_active, 0, 60000, "Get/Set event active duration in milliseconds.", 100),
	CTR_CONFIG_ITEM_INT("trigger-duration-inactive", m_config_interim.trigger_duration_inactive, 0, 60000, "Get/Set event inactive duration in milliseconds.", 100),
	CTR_CONFIG_ITEM_INT("trigger-cooldown-time", m_config_interim.trigger_cooldown_time, 0, 60000, "Get/Set trigger cooldown time in milliseconds.", 10),
	CTR_CONFIG_ITEM_BOOL("trigger-report-active", m_config_interim.trigger_report_active, "Get/Set report when trigger is active.", false),
	CTR_CONFIG_ITEM_BOOL("trigger-report-inactive", m_config_interim.trigger_report_inactive, "Get/Set report when trigger is inactive.", false),
	CTR_CONFIG_ITEM_INT("counter-interval-aggreg", m_config_interim.counter_interval_aggreg, 1, 86400, "Get/Set counter aggregation interval in seconds.", 300),
	CTR_CONFIG_ITEM_INT("counter-duration-active", m_config_interim.counter_duration_active, 0, 60000, "Get/Set event active duration in milliseconds.", 2),
	CTR_CONFIG_ITEM_INT("counter-duration-inactive", m_config_interim.counter_duration_inactive, 0, 60000, "Get/Set event inactive duration in milliseconds.", 2),
	CTR_CONFIG_ITEM_INT("counter-cooldown-time", m_config_interim.counter_cooldown_time, 0, 60000, "Get/Set counter cooldown time in milliseconds.", 10),
	CTR_CONFIG_ITEM_INT("analog-interval-sample", m_config_interim.analog_interval_sample, 1, 86400, "Get/Set analog sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("analog-interval-aggreg", m_config_interim.analog_interval_aggreg, 1, 86400, "Get/Set analog aggregation interval in seconds.", 300),
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	CTR_CONFIG_ITEM_INT("hygro-interval-sample", m_config_interim.hygro_interval_sample, 1, 86400, "Get/Set hygro sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("hygro-interval-aggreg", m_config_interim.hygro_interval_aggreg, 1, 86400, "Get/Set hygro aggreg interval in seconds.", 300),
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	CTR_CONFIG_ITEM_INT("w1-therm-interval-sample", m_config_interim.w1_therm_interval_sample, 1, 86400, "Get/Set ds18b20 sample interval in seconds.", 60),
	CTR_CONFIG_ITEM_INT("w1-therm-interval-aggreg", m_config_interim.w1_therm_interval_aggreg, 1, 86400, "Get/Set ds18b20 aggreg interval in seconds .", 300),
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

	CTR_CONFIG_ITEM_ENUM("mode", m_config_interim.mode, ((const char*[]){"none", "lte", "lrw"}), "Set communication mode", APP_CONFIG_MODE_NONE),

	/* ### Preserved code "config" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	CTR_CONFIG_ITEM_STRING_PARSE_CB("sensor-1", m_config_interim.sensor_str[0],
		"Sensor 1: type,ch_power,ch_measure (e.g. ld-81,2,1)", "", sensor_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("sensor-2", m_config_interim.sensor_str[1],
		"Sensor 2: type,ch_power,ch_measure", "", sensor_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("sensor-3", m_config_interim.sensor_str[2],
		"Sensor 3: type,ch_power,ch_measure", "", sensor_parse_cb),
	CTR_CONFIG_ITEM_STRING_PARSE_CB("sensor-4", m_config_interim.sensor_str[3],
		"Sensor 4: type,ch_power,ch_measure", "", sensor_parse_cb),
	/* Bounded by the LD-81 fail-safe window: outside it a flood can never be detected. */
	CTR_CONFIG_ITEM_INT("sensor-ld81-threshold", m_config_interim.ld81_threshold_mv,
		LD81_THRESHOLD_MIN_MV, LD81_THRESHOLD_MAX_MV,
		"Get/Set LD-81 flood threshold in mV (flood = above).", 1100),
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
	/* ^^^ Preserved code "config" (end) */

};
/* clang-format on */

/* ### Preserved code "function" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
/* Parse "type,ch_power,ch_measure" (1-based channels) into a sensor slot. */
/* Strict decimal channel number: `(uint8_t)atoi()` would let "258" wrap to channel 2
 * and "x" become channel 0. The range itself is checked by the validator. */
static int parse_sensor_channel(const char *token, uint8_t *ch)
{
	char *end;
	long v = strtol(token, &end, 10);

	if (end == token || *end != '\0' || v < 0 || v > UINT8_MAX) {
		return -EINVAL;
	}

	*ch = (uint8_t)v;

	return 0;
}

static int parse_sensor_string(const char *input, struct app_config_sensor *s)
{
	char buf[APP_CONFIG_SENSOR_STR_SIZE];
	char *token;
	char *saveptr;

	if (!s) {
		return -EINVAL;
	}

	s->type_id = APP_SENSOR_TYPE_NONE;
	s->ch_power = 0;
	s->ch_measure = 0;

	if (!input || !input[0]) {
		return 0;
	}

	strncpy(buf, input, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	token = strtok_r(buf, ",", &saveptr);
	if (!token || strcmp(token, "none") == 0) {
		return 0;
	}

	const struct app_sensor_driver *drv = app_sensor_find_driver_by_name(token);
	if (!drv) {
		return -EINVAL;
	}
	s->type_id = drv->type_id;

	token = strtok_r(NULL, ",", &saveptr);
	if (token && parse_sensor_channel(token, &s->ch_power)) {
		return -EINVAL;
	}

	token = strtok_r(NULL, ",", &saveptr);
	if (token && parse_sensor_channel(token, &s->ch_measure)) {
		return -EINVAL;
	}

	/* Trailing fields are a typo, not something to ignore silently. */
	if (strtok_r(NULL, ",", &saveptr)) {
		return -EINVAL;
	}

	return 0;
}

static int validate_sensor_slot(const struct shell *shell, const struct app_config_sensor *s,
				const void *self);
static void sensor_claim_channels(const struct shell *shell, const struct app_config_sensor *s);
static enum app_config_channel_mode config_channel_mode(const struct app_config *cfg, int ch_idx);
static void config_set_channel_mode(struct app_config *cfg, int ch_idx,
				    enum app_config_channel_mode mode);

/* Which slot an item belongs to, from the string buffer it points at. The config
 * items are declared against `m_config_interim.sensor_str[N]`, so the pointer is
 * the only link back to N. Returns -1 if it is not one of them. */
static int sensor_slot_of_item(const struct ctr_config_item *item)
{
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		if (item->variable == (void *)m_config_interim.sensor_str[i]) {
			return i;
		}
	}

	return -1;
}

/* Keep the parsed slot in step with the string that was just stored. `parsed` may
 * be NULL to clear the slot. */
static void sensor_store_parsed(const struct ctr_config_item *item,
				const struct app_config_sensor *parsed)
{
	int slot = sensor_slot_of_item(item);

	if (slot < 0) {
		return;
	}

	if (parsed) {
		m_config_interim.sensors[slot] = *parsed;
	} else {
		memset(&m_config_interim.sensors[slot], 0,
		       sizeof(m_config_interim.sensors[slot]));
		m_config_interim.sensors[slot].type_id = APP_SENSOR_TYPE_NONE;
	}
}

static int sensor_parse_cb(const struct shell *shell, char *argv,
			   const struct ctr_config_item *item)
{
	size_t len = strlen(argv);

	if (len >= item->size) {
		shell_error(shell, "Value too long (max %d)", item->size - 1);
		return -ENOMEM;
	}

	if (len == 0) {
		memset(item->variable, 0, item->size);
		sensor_store_parsed(item, NULL);
		return 0;
	}

	struct app_config_sensor test;
	int ret = parse_sensor_string(argv, &test);
	if (ret) {
		shell_error(shell, "Invalid format. Use: type,ch_power,ch_measure");
		shell_print(shell, "  Types: none, ld-81");
		return ret;
	}

	ret = validate_sensor_slot(shell, &test, item->variable);
	if (ret) {
		return ret;
	}

	strncpy(item->variable, argv, item->size - 1);
	((char *)item->variable)[item->size - 1] = '\0';

	/* Keep the interim config internally consistent: `sensor_str[]` and `sensors[]`
	 * are two representations of the same setting and should not disagree.
	 *
	 * This deliberately does NOT touch `g_app_config`, so it does not make the
	 * running system see the new sensor — `app_config_channel_owner()` and the
	 * sensor runtime read `g_app_config`, which only h_commit() updates, on
	 * settings_load. That asymmetry is intentional: the sensor layer set up its
	 * channels and ADC at init, so flipping ownership underneath it would mark a
	 * channel as sensor-owned while no sensor is driving it, and the raw sampler
	 * would then skip a channel that nothing samples — worse than waiting for the
	 * reboot that top-level `config save` performs anyway. */
	sensor_store_parsed(item, &test);

	sensor_claim_channels(shell, &test);

	return 0;
}

/* Configuring a sensor also configures the two channels it drives, so the user
 * only has to type the sensor line.
 *
 * There is deliberately no counterpart that releases channels when a sensor is
 * cleared or re-pointed. Setting a channel back to `disabled` on the user's
 * behalf can break a working unit: correct readings appeared to depend on at
 * least one channel being configured as `voltage` (an unmeasured channel read 11x
 * high), so silently disabling one is not a safe thing to do automatically. That
 * was observed before the ctr_adc input mapping fix (a317b690) and may have been
 * its off-by-one; it has not been re-checked on hardware since. Stale modes left
 * behind by re-pointing a sensor are harmless: ownership is derived at init, and a
 * `power` mode no sensor claims is treated as disabled there. The measure channel is
 * marked `voltage` and the power channel `power`, which is what the LRW payload and
 * `config show` then report — the pair describes itself rather than both reading
 * `power`.
 *
 * The modes are written into the interim config, so they are visible in
 * `app config show` straight away and persist on the next `config save`. */
static void sensor_claim_channels(const struct shell *shell, const struct app_config_sensor *s)
{
	const struct app_sensor_driver *drv = app_sensor_find_driver_by_type(s->type_id);

	if (!drv) {
		return;
	}

	if (s->ch_measure >= 1 && s->ch_measure <= APP_DATA_NUM_CHANNELS) {
		config_set_channel_mode(&m_config_interim, s->ch_measure - 1,
					APP_CONFIG_CHANNEL_MODE_VOLTAGE);
		shell_print(shell, "channel-mode-%u set to \"voltage\" (sensor measure)",
			    s->ch_measure);
	}

	if (drv->needs_power && s->ch_power >= 1 && s->ch_power <= APP_DATA_NUM_CHANNELS) {
		config_set_channel_mode(&m_config_interim, s->ch_power - 1,
					APP_CONFIG_CHANNEL_MODE_POWER);
		shell_print(shell, "channel-mode-%u set to \"power\" (sensor supply)", s->ch_power);
	}
}

/* Channel mode (0-based index) from a given config — the modes are stored as
 * separate fields, not an array. */
static enum app_config_channel_mode config_channel_mode(const struct app_config *cfg, int ch_idx)
{
	switch (ch_idx) {
	case 0: return cfg->channel_mode_1;
	case 1: return cfg->channel_mode_2;
	case 2: return cfg->channel_mode_3;
	case 3: return cfg->channel_mode_4;
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	case 4: return cfg->channel_mode_5;
	case 5: return cfg->channel_mode_6;
	case 6: return cfg->channel_mode_7;
	case 7: return cfg->channel_mode_8;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_B) */
	default: return APP_CONFIG_CHANNEL_MODE_DISABLED;
	}
}

/* Counterpart of `config_channel_mode` — writes the mode back into the per-channel
 * field for a 0-based channel index. Out-of-range indices are ignored rather than
 * flagged: callers derive the index from an already-validated sensor slot. */
static void config_set_channel_mode(struct app_config *cfg, int ch_idx,
				    enum app_config_channel_mode mode)
{
	switch (ch_idx) {
	case 0: cfg->channel_mode_1 = mode; break;
	case 1: cfg->channel_mode_2 = mode; break;
	case 2: cfg->channel_mode_3 = mode; break;
	case 3: cfg->channel_mode_4 = mode; break;
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	case 4: cfg->channel_mode_5 = mode; break;
	case 5: cfg->channel_mode_6 = mode; break;
	case 6: cfg->channel_mode_7 = mode; break;
	case 7: cfg->channel_mode_8 = mode; break;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_B) */
	default: break;
	}
}

enum app_config_channel_mode app_config_get_channel_mode(int ch_idx)
{
	return config_channel_mode(&g_app_config, ch_idx);
}

bool app_config_sensor_channels_valid(const struct app_config_sensor *s)
{
	if (!s || s->type_id == APP_SENSOR_TYPE_NONE) {
		return false;
	}

	const struct app_sensor_driver *drv = app_sensor_find_driver_by_type(s->type_id);

	if (!drv) {
		return false;
	}

	if (s->ch_measure < 1 || s->ch_measure > APP_DATA_NUM_CHANNELS) {
		return false;
	}

	if (drv->needs_power) {
		if (s->ch_power < 1 || s->ch_power > APP_DATA_NUM_CHANNELS) {
			return false;
		}
		/* Same channel for power and measure would have the measurement mode
		 * immediately overwrite the power mode, so the sensor is never
		 * excited and reads dry forever — the worst failure for an alarm. */
		if (s->ch_power == s->ch_measure) {
			return false;
		}
	}

	return true;
}

int app_config_channel_owner(int ch_num)
{
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		const struct app_config_sensor *s = &g_app_config.sensors[i];
		if (s->type_id == APP_SENSOR_TYPE_NONE) {
			continue;
		}
		if (s->ch_measure == ch_num || s->ch_power == ch_num) {
			return i;
		}
	}
	return -1;
}

/* True when a channel mode drives the raw X0 sampling path, and would therefore
 * sample a channel that a sensor also drives. `disabled` is free to claim, and
 * `power` already means "driven by the sensor layer". */
static bool channel_mode_conflicts(enum app_config_channel_mode mode)
{
	switch (mode) {
	case APP_CONFIG_CHANNEL_MODE_TRIGGER:
	case APP_CONFIG_CHANNEL_MODE_COUNTER:
	case APP_CONFIG_CHANNEL_MODE_VOLTAGE:
	case APP_CONFIG_CHANNEL_MODE_CURRENT:
		return true;
	default:
		return false;
	}
}

/* Validate one sensor slot as it is being set. `self` is that slot's own string
 * storage, so the slot being replaced is not compared against its old value.
 *
 * A conflicting channel-mode-N is only a warning, not an error: ownership is
 * derived at init (see `init_chester_x0`), so the sensor always wins and the raw
 * X0 path skips the channel. Rejecting here would break existing deployments
 * whose channel-mode-N was set by hand before the sensor layer existed. */
static int validate_sensor_slot(const struct shell *shell, const struct app_config_sensor *s,
				const void *self)
{
	if (s->type_id == APP_SENSOR_TYPE_NONE) {
		return 0;
	}

	const struct app_sensor_driver *drv = app_sensor_find_driver_by_type(s->type_id);
	uint8_t chans[2];
	int nchans = 0;

	if (s->ch_measure < 1 || s->ch_measure > APP_DATA_NUM_CHANNELS) {
		shell_error(shell, "ch_measure %d out of range (1-%d)", s->ch_measure,
			    APP_DATA_NUM_CHANNELS);
		return -EINVAL;
	}
	chans[nchans++] = s->ch_measure;

	if (drv->needs_power) {
		if (s->ch_power < 1 || s->ch_power > APP_DATA_NUM_CHANNELS) {
			shell_error(shell, "ch_power %d out of range (1-%d)", s->ch_power,
				    APP_DATA_NUM_CHANNELS);
			return -EINVAL;
		}
		if (s->ch_power == s->ch_measure) {
			shell_error(shell, "ch_power == ch_measure");
			return -EINVAL;
		}
		chans[nchans++] = s->ch_power;
	}

	for (int i = 0; i < nchans; i++) {
		int ch = chans[i];

		for (int j = 0; j < APP_SENSORS_MAX; j++) {
			if ((const void *)m_config_interim.sensor_str[j] == self) {
				continue;
			}

			struct app_config_sensor other;

			if (parse_sensor_string(m_config_interim.sensor_str[j], &other)) {
				continue;
			}
			if (other.type_id == APP_SENSOR_TYPE_NONE) {
				continue;
			}
			if (other.ch_measure == ch || other.ch_power == ch) {
				shell_error(shell, "channel %d is already claimed by sensor-%d",
					    ch, j + 1);
				return -EINVAL;
			}
		}

		if (channel_mode_conflicts(config_channel_mode(&m_config_interim, ch - 1))) {
			shell_warn(shell,
				   "channel %d also has channel-mode-%d set; the sensor takes "
				   "over the channel and the raw reading is not reported",
				   ch, ch);
		}
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
/* ^^^ Preserved code "function" (end) */

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	/* ### Preserved code "app_config_cmd_config_show start" (begin) */
	/* ^^^ Preserved code "app_config_cmd_config_show start" (end) */

	for (int i = 0; i < ARRAY_SIZE(items); i++) {
		ctr_config_show_item(shell, &items[i]);
	}

	/* ### Preserved code "app_config_cmd_config_show end" (begin) */
	/* ^^^ Preserved code "app_config_cmd_config_show end" (end) */

	return 0;
}

int app_config_cmd_config(const struct shell *shell, size_t argc, char **argv)
{
	/* ### Preserved code "app_config_cmd_config" (begin) */
	/* Sensor slots are validated in `sensor_parse_cb` as each one is set.
	 * Do not hook validation on `argv[1] == "save"` here: saving is the
	 * top-level `config save` command, which calls `settings_save()`
	 * directly and never reaches this function. */
	/* ^^^ Preserved code "app_config_cmd_config" (end) */
	return ctr_config_cmd_config(items, ARRAY_SIZE(items), shell, argc, argv);
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	/* ### Preserved code "h_set" (begin) */
	/* ^^^ Preserved code "h_set" (end) */

	return ctr_config_h_set(items, ARRAY_SIZE(items), key, len, read_cb, cb_arg);
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");

	/* ### Preserved code "h_commit" (begin) */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		struct app_config_sensor *s = &m_config_interim.sensors[i];

		if (parse_sensor_string(m_config_interim.sensor_str[i], s)) {
			LOG_WRN("sensor-%d: unparsable stored value; ignoring", i + 1);
			s->type_id = APP_SENSOR_TYPE_NONE;
			continue;
		}

		/* This path loads straight from flash and does not run the shell
		 * path's validate_sensor_slot(), so a value written by an older or
		 * hand-edited build could carry channels that are 0, out of range, or
		 * the same for power and measure. Drop the slot rather than let the
		 * runtime index a channel lookup table out of bounds, or run a sensor
		 * that can never be excited. */
		if (s->type_id != APP_SENSOR_TYPE_NONE &&
		    !app_config_sensor_channels_valid(s)) {
			LOG_WRN("sensor-%d: unusable stored channels (power %u, measure %u); "
				"ignoring",
				i + 1, s->ch_power, s->ch_measure);
			s->type_id = APP_SENSOR_TYPE_NONE;
			continue;
		}

		/* The shell path refuses a channel another sensor already claims, but a
		 * stored config bypasses it. Two sensors on one channel would drive it
		 * against each other, so the later slot loses. */
		for (int j = 0; s->type_id != APP_SENSOR_TYPE_NONE && j < i; j++) {
			const struct app_config_sensor *o = &m_config_interim.sensors[j];

			if (o->type_id == APP_SENSOR_TYPE_NONE) {
				continue;
			}
			if ((s->ch_power && (s->ch_power == o->ch_power ||
					     s->ch_power == o->ch_measure)) ||
			    s->ch_measure == o->ch_power || s->ch_measure == o->ch_measure) {
				LOG_WRN("sensor-%d: channels already claimed by sensor-%d; "
					"ignoring",
					i + 1, j + 1);
				s->type_id = APP_SENSOR_TYPE_NONE;
			}
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
	/* ^^^ Preserved code "h_commit" (end) */

	memcpy(&g_app_config, &m_config_interim, sizeof(g_app_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
	/* ### Preserved code "h_export" (begin) */
	/* ^^^ Preserved code "h_export" (end) */

	return ctr_config_h_export(items, ARRAY_SIZE(items), export_func);
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	for (int i = 0; i < ARRAY_SIZE(items); i++) {
		ctr_config_init_item(&items[i]);
	}

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

	/* ### Preserved code "init" (begin) */
	/* ^^^ Preserved code "init" (end) */

	ctr_config_append_show(SETTINGS_PFX, app_config_cmd_config_show);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
