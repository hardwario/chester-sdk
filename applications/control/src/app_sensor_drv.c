/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_sensor_drv.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_x0_map.h"
#include "feature.h"
#include "drivers/drv_ld81.h"

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(app_sensor_drv, LOG_LEVEL_INF);

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

/* Generic settle time used when a driver doesn't request its own. */
#define APP_SENSOR_DEFAULT_SETTLE_MS 100

/* Ceiling on a believable reading from an X0 analog input on this hardware. The
 * excitation rail is at most 5 V, so with the 100k/10k divider in circuit and the
 * ×11 compensation applied, a sensor node cannot report much above that. A reading
 * far beyond it means the divider was not in circuit and the ×11 was applied to
 * the bare node voltage — see the retry loop in `app_sensor_drv_sample_slot`. */
#define APP_SENSOR_MAX_PLAUSIBLE_MV 5500

/* Re-drives the divider control pins this many extra times before discarding the
 * sample. Two retries cost ~100 ms in the failure case and nothing at all in the
 * normal one. */
#define APP_SENSOR_DIVIDER_RETRIES           2
#define APP_SENSOR_DIVIDER_RETRY_SETTLE_MS   50

/* Burst used to tell a held node from a floating one — see `max_spread_mv`. Short
 * enough that a real flood cannot come and go inside it. */
#define APP_SENSOR_STABILITY_READS  4
#define APP_SENSOR_STABILITY_GAP_MS 30

/* Single definition of the X0 channel/ADC maps (declared in app_x0_map.h);
 * 0-based channel index -> per-module enum. */
const enum ctr_x0_channel app_x0_channel_map[APP_DATA_NUM_CHANNELS] = {
	CTR_X0_CHANNEL_1, CTR_X0_CHANNEL_2, CTR_X0_CHANNEL_3, CTR_X0_CHANNEL_4,
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	CTR_X0_CHANNEL_1, CTR_X0_CHANNEL_2, CTR_X0_CHANNEL_3, CTR_X0_CHANNEL_4,
#endif
};

const enum ctr_adc_channel app_x0_adc_map[APP_DATA_NUM_CHANNELS] = {
	CTR_ADC_CHANNEL_A0, CTR_ADC_CHANNEL_A1, CTR_ADC_CHANNEL_A2, CTR_ADC_CHANNEL_A3,
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	CTR_ADC_CHANNEL_B0, CTR_ADC_CHANNEL_B1, CTR_ADC_CHANNEL_B2, CTR_ADC_CHANNEL_B3,
#endif
};

/* Device that owns a 1-based channel (1..4 -> X0-A, 5..8 -> X0-B). */
static const struct device *ch_device(uint8_t ch1)
{
	static const struct device *dev_a = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));
#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	static const struct device *dev_b = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_b));
	if (ch1 >= 5) {
		return dev_b;
	}
#endif
	return dev_a;
}

/* The X0 mode the application holds this 1-based channel in between samples.
 *
 * Sensor ownership comes first, from the boot-derived g_app_data.sensors (not the live
 * config, which may hold an unapplied edit): a supply channel is parked in DEFAULT and
 * only driven during a sample, a measure channel is held in AI_INPUT. Otherwise the
 * configured mode decides, mirroring init: `voltage` is AI_INPUT and `current` is
 * CL_INPUT. Restoring a 4-20 mA channel to AI_INPUT instead would drop its current-loop
 * shunt and break its readings until the next reboot. */
static enum ctr_x0_mode ch_held_mode(int ch)
{
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		const struct app_data_sensor *s = &g_app_data.sensors[i];

		if (!s->configured) {
			continue;
		}
		if (s->ch_power == ch) {
			return CTR_X0_MODE_DEFAULT;
		}
		if (s->ch_measure == ch) {
			return CTR_X0_MODE_AI_INPUT;
		}
	}

	switch (app_config_get_channel_mode(ch - 1)) {
	case APP_CONFIG_CHANNEL_MODE_VOLTAGE:
		return CTR_X0_MODE_AI_INPUT;
	case APP_CONFIG_CHANNEL_MODE_CURRENT:
		return CTR_X0_MODE_CL_INPUT;
	default:
		return CTR_X0_MODE_DEFAULT;
	}
}

/* True if `sensor scan` must leave this 0-based channel alone.
 *
 * Digital inputs are held in NPN_INPUT or PNP_INPUT for the life of the app:
 * driving one to AI_INPUT and then releasing it to DEFAULT drops the pull that
 * makes the input work, silently killing a trigger or counter until the next
 * reboot. Reproducing init's NPN/PNP choice here would just be a second copy to
 * drift.
 *
 * A `current` channel is held in CL_INPUT for its 4-20 mA shunt, and the normal
 * sampler keeps reading it while a scan runs. Switching it to AI_INPUT for the scan
 * would take the shunt off the loop for those seconds, and the sampler would record
 * whatever the bare input then reads. A sensor is never wired to a current loop
 * anyway, so the scan has nothing to learn there. */
static bool ch_scan_skips(int ch_idx)
{
	enum app_config_channel_mode mode = app_config_get_channel_mode(ch_idx);

	return mode == APP_CONFIG_CHANNEL_MODE_TRIGGER || mode == APP_CONFIG_CHANNEL_MODE_COUNTER ||
	       mode == APP_CONFIG_CHANNEL_MODE_CURRENT;
}

/* True if this 1-based channel may carry the scan's excitation: a configured sensor's
 * supply, or a channel set to `disabled` or `power`. Driving any other channel pushes
 * the excitation into whatever is connected to it, such as a voltage source or
 * another sensor's measure line. */
static bool ch_scan_may_supply(int ch)
{
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		const struct app_data_sensor *s = &g_app_data.sensors[i];

		if (s->configured && s->ch_power == ch) {
			return true;
		}
	}

	enum app_config_channel_mode mode = app_config_get_channel_mode(ch - 1);

	return mode == APP_CONFIG_CHANNEL_MODE_DISABLED || mode == APP_CONFIG_CHANNEL_MODE_POWER;
}

/* Re-drive the divider control pins of every channel the application holds in
 * AI_INPUT or CL_INPUT, each back to its own mode, stepping through DEFAULT so the
 * writes cannot be short-circuited by `ctr_x0_set_mode`'s per-channel mode cache.
 * Supply channels are held in DEFAULT and are skipped, so this never touches the
 * excitation of the slot being sampled.
 *
 * The whole set, not just the failing channel, because on the bench a channel
 * only read correctly divided while ANOTHER channel's divider was engaged (ch3
 * depended on ch1). That was measured on 2026-08-14, before the ctr_adc input
 * mapping fix (a317b690): with its off-by-one every channel read its neighbour's
 * AIN, and ch3 (A2) read ch1's pin, which accounts for the coupling. It is kept
 * until it is re-checked on hardware with the fixed ctr_adc. Re-driving a handful
 * of expander pins costs a few I2C writes on a path that only runs when a reading
 * is already impossible. */
static void x0_redrive_engaged(const struct device *mdev, enum ctr_x0_channel mch,
			       uint8_t ch_measure)
{
	ctr_x0_set_mode(mdev, mch, CTR_X0_MODE_DEFAULT);
	ctr_x0_set_mode(mdev, mch, CTR_X0_MODE_AI_INPUT);

	for (int ch = 1; ch <= APP_DATA_NUM_CHANNELS; ch++) {
		if (ch == ch_measure) {
			continue;
		}

		enum ctr_x0_mode mode = ch_held_mode(ch);

		if (mode != CTR_X0_MODE_AI_INPUT && mode != CTR_X0_MODE_CL_INPUT) {
			continue;
		}

		ctr_x0_set_mode(ch_device(ch), app_x0_channel_map[ch - 1], CTR_X0_MODE_DEFAULT);
		ctr_x0_set_mode(ch_device(ch), app_x0_channel_map[ch - 1], mode);
	}
}

/* Serializes the power-on -> settle -> read -> power-off sequence. Two threads
 * drive the same channels: the work queue (analog_sample_work_handler ->
 * app_sensor_drv_sample_all) and the shell thread (`sensor read`). Without this
 * one could park a channel back to DEFAULT, or drive the power channel, while
 * the other is mid-settle or mid-read — the ADC would then sample a channel that
 * is not in the state it expects, giving a garbage voltage: a spurious flood
 * alarm, or a real transition missed. The driver's own lock only guards its
 * decoded data, not the hardware sequence. */
static K_MUTEX_DEFINE(m_sample_lock);

/* --- catalog / dispatch (no per-type logic here) --- */

const struct app_sensor_driver *app_sensor_find_driver_by_name(const char *name)
{
	if (!name) {
		return NULL;
	}
	if (strcmp(name, "ld-81") == 0) {
		return &ld81_driver;
	}
	/* future types: add name matches here */
	return NULL;
}

const struct app_sensor_driver *app_sensor_find_driver_by_type(uint8_t type_id)
{
	switch (type_id) {
	case APP_SENSOR_TYPE_LD81:
		return &ld81_driver;
	/* future types: add cases here */
	default:
		return NULL;
	}
}

/* --- runtime --- */

/* Hand a discarded sample to the driver. A fault found here (floating input, divider
 * not engaged, X0 not responding, ADC error) loses the reading just as an open sense
 * line does, so the driver may ask for it to be reported now: return its alarm in
 * place of `err`, so the caller dispatches the report. */
static int sample_discard(const struct app_sensor_driver *drv, int i, int err)
{
	if (drv->invalidate && drv->invalidate(i) == APP_SENSOR_SAMPLE_ALARM) {
		return APP_SENSOR_SAMPLE_ALARM;
	}

	return err;
}

/* Set up a slot's ADC input, measure channel and driver. Run at boot, and again before
 * each sample for as long as it fails, so a slot whose ADC input or driver did not come
 * up recovers without a reboot. `verbose` is false for those retries: the boot already
 * logged the failure, and repeating it every analog interval would bury the log. */
static int slot_setup(int i, const struct app_sensor_driver *drv, bool verbose)
{
	struct app_data_sensor *slot = &g_app_data.sensors[i];

	int ret = ctr_adc_init(app_x0_adc_map[slot->ch_measure - 1]);
	if (ret) {
		if (verbose) {
			LOG_ERR("sensor-%d: `ctr_adc_init` failed: %d", i + 1, ret);
		}
		return ret;
	}

	/* Engage the measure channel's input divider up front, so the first
	 * sample after boot is already correctly scaled rather than relying on
	 * the sample path to discover it is not.
	 *
	 * AI_INPUT switches the X0's 100k/10k divider into circuit, and
	 * `CTR_ADC_X0_AI_MILLIVOLTS` multiplies by 11 unconditionally to
	 * compensate. When the divider is not in circuit the reading comes back
	 * exactly 11x high — an 8.8 V "measurement" on a 3.6 V board, above any
	 * flood threshold and so reported as a permanent flood. This used to be
	 * invisible in the obvious place: it only appeared when no *other* channel
	 * happened to be set to `voltage`, because `init_chester_x0_voltages`
	 * leaves that channel's divider permanently engaged. Disabling an
	 * apparently unrelated channel silently broke flood detection. (Seen before
	 * the ctr_adc input mapping fix a317b690, whose off-by-one read the
	 * neighbouring AIN; not re-checked on hardware since.)
	 *
	 * Leaving it engaged costs nothing measurable: the divider is 110k across
	 * a node that is only driven while the supply channel pulses, and the board
	 * measured 100 uA average with a divider permanently engaged. It is not
	 * sufficient on its own, though — see the retry loop in
	 * `app_sensor_drv_sample_slot` for the case where it drops back out. */
	ret = ctr_x0_set_mode(ch_device(slot->ch_measure), app_x0_channel_map[slot->ch_measure - 1],
			      CTR_X0_MODE_AI_INPUT);
	if (ret && verbose) {
		/* Not a setup failure: the sample path asserts the mode again and
		 * invalidates on failure, so a module that enumerates late still
		 * recovers. */
		LOG_WRN("sensor-%d: cannot hold measure ch%u as analog input: %d", i + 1,
			slot->ch_measure, ret);
	}

	if (drv->init) {
		ret = drv->init(i, slot->ch_power, slot->ch_measure);
		if (ret) {
			if (verbose) {
				LOG_WRN("sensor-%d: driver init failed: %d", i + 1, ret);
			}
			return ret;
		}
	}

	slot->ready = true;

	return 0;
}

int app_sensor_drv_init(void)
{
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		struct app_config_sensor *cfg = &g_app_config.sensors[i];
		struct app_data_sensor *slot = &g_app_data.sensors[i];

		slot->configured = false;
		slot->ready = false;

		if (cfg->type_id == APP_SENSOR_TYPE_NONE) {
			continue;
		}

		const struct app_sensor_driver *drv = app_sensor_find_driver_by_type(cfg->type_id);
		if (!drv) {
			LOG_WRN("sensor-%d: unknown type %u", i + 1, cfg->type_id);
			continue;
		}

		/* Defensive: h_commit() already drops unusable slots, but this is the
		 * last point before the channel numbers index the lookup tables. */
		if (!app_config_sensor_channels_valid(cfg)) {
			LOG_ERR("sensor-%d: unusable channels (power %u, measure %u); ignoring",
				i + 1, cfg->ch_power, cfg->ch_measure);
			continue;
		}

		/* Configured even if the setup below fails. The slot's channels are claimed
		 * either way, because ownership comes from the config, so dropping the slot
		 * would only make the sensor vanish from the uplink while `channel show`
		 * still lists it as the owner. It is reported invalid instead, and the setup
		 * is retried before each sample. */
		slot->configured = true;
		slot->type_id = cfg->type_id;
		slot->ch_power = cfg->ch_power;
		slot->ch_measure = cfg->ch_measure;

		if (slot_setup(i, drv, true)) {
			LOG_ERR("sensor-%d: setup failed; reporting it invalid and retrying "
				"before each sample",
				i + 1);
			continue;
		}

		LOG_INF("sensor-%d: %s (power ch%u, measure ch%u)", i + 1, drv->name, cfg->ch_power,
			cfg->ch_measure);
	}

	return 0;
}

/* Power-on -> settle -> ADC read -> power-off for one configured slot. */
int app_sensor_drv_sample_slot(int i)
{
	if (i < 0 || i >= APP_SENSORS_MAX) {
		return -EINVAL;
	}

	struct app_data_sensor *slot = &g_app_data.sensors[i];

	if (!slot->configured) {
		return -ENODEV;
	}

	const struct app_sensor_driver *drv = app_sensor_find_driver_by_type(slot->type_id);
	if (!drv || !drv->sample) {
		return -ENOTSUP;
	}

	const struct device *mdev = ch_device(slot->ch_measure);
	enum ctr_x0_channel mch = app_x0_channel_map[slot->ch_measure - 1];
	const struct device *pdev = NULL;
	enum ctr_x0_channel pch = 0;

	/* Hold the lock across the whole hardware sequence — see m_sample_lock. */
	k_mutex_lock(&m_sample_lock, K_FOREVER);

	if (!slot->ready) {
		int err = slot_setup(i, drv, false);

		if (err) {
			err = sample_discard(drv, i, err);
			k_mutex_unlock(&m_sample_lock);
			return err;
		}

		LOG_INF("sensor-%d: setup succeeded on retry", i + 1);
	}

	/* Power on (switched), sense as analog input.
	 *
	 * These have to be checked. The ADC lives on the SoC, not on the X0, so if the
	 * module is missing or failed to enumerate every `ctr_x0_set_mode` returns
	 * -ENODEV while `ctr_adc_read` still happily returns a value -- for a pin that
	 * was never configured. That floating reading then gets scaled by the ×11
	 * divider factor and reported as a real measurement, which is how a detached
	 * module turns into a confident flood alarm. Treat it as an invalid sample
	 * instead, the same as an ADC failure. */
	int ret = 0;

	if (drv->needs_power) {
		pdev = ch_device(slot->ch_power);
		pch = app_x0_channel_map[slot->ch_power - 1];
		ret = ctr_x0_set_mode(pdev, pch, CTR_X0_MODE_PWR_SOURCE);
	}

	/* Force the analog-input mode to be written to the hardware, rather than
	 * trusting that it is still applied.
	 *
	 * `ctr_x0_set_mode` caches the last mode per channel and returns immediately
	 * when the requested mode matches, so it never rewrites the GPIOs. That cache
	 * is a model of what we asked for, not of what the expander currently holds:
	 * if the divider control pin is disturbed once, the cache says AI_INPUT
	 * forever and nothing ever repairs it. Stepping through DEFAULT guarantees the
	 * pins are actually driven again on every sample. */
	if (!ret) {
		ret = ctr_x0_set_mode(mdev, mch, CTR_X0_MODE_DEFAULT);
	}

	if (!ret) {
		ret = ctr_x0_set_mode(mdev, mch, CTR_X0_MODE_AI_INPUT);
	}

	if (ret) {
		LOG_ERR("sensor-%d: cannot configure channels (X0 module not ready?): %d",
			i + 1, ret);
		/* The supply may already be on if only the measure channel failed; do not
		 * leave the excitation rail powered until the next sample. */
		if (drv->needs_power) {
			ctr_x0_set_mode(pdev, pch, CTR_X0_MODE_DEFAULT);
		}
		ret = sample_discard(drv, i, ret);
		k_mutex_unlock(&m_sample_lock);
		return ret;
	}

	k_msleep(drv->power_on_delay_ms ? drv->power_on_delay_ms : APP_SENSOR_DEFAULT_SETTLE_MS);

	/* Read, then sanity-check the scaling and retry if it is wrong.
	 *
	 * `CTR_ADC_X0_AI_MILLIVOLTS` multiplies by 11 to undo the X0's 100k/10k
	 * divider, which AI_INPUT is supposed to switch into circuit. When the divider
	 * is not actually in circuit the ADC sees the undivided node and the ×11 turns
	 * a real 0.8 V into a reported 8.8 V — above every threshold, so it reads as a
	 * permanent flood. A value the hardware cannot produce is therefore not a
	 * measurement but proof that the divider dropped out, and re-driving the pins
	 * is worth a try before giving up on the sample. */
	uint16_t raw;
	int mv = 0;
	int attempt;

	for (attempt = 0; attempt <= APP_SENSOR_DIVIDER_RETRIES; attempt++) {
		ret = ctr_adc_read(app_x0_adc_map[slot->ch_measure - 1], &raw);
		if (ret) {
			break;
		}

		mv = (int)CTR_ADC_X0_AI_MILLIVOLTS(raw);

		if (mv <= APP_SENSOR_MAX_PLAUSIBLE_MV) {
			if (attempt) {
				LOG_INF("sensor-%d: divider re-engaged after %d attempt(s), "
					"%d mV",
					i + 1, attempt, mv);
			}
			break;
		}

		if (attempt < APP_SENSOR_DIVIDER_RETRIES) {
			LOG_WRN("sensor-%d: %d mV is above the %d mV the hardware can produce "
				"— the ×11 input divider is not engaged; re-driving the "
				"engaged channels",
				i + 1, mv, APP_SENSOR_MAX_PLAUSIBLE_MV);
			x0_redrive_engaged(mdev, mch, slot->ch_measure);
			k_msleep(APP_SENSOR_DIVIDER_RETRY_SETTLE_MS);
		}
	}

	/* Confirm the node is actually being held at that voltage, not just passing
	 * through it. An input with nothing connected drifts over hundreds of
	 * millivolts, and that noise can sit anywhere — including above the flood
	 * threshold but below the open-circuit ceiling, where it is indistinguishable
	 * from a real flood. A short burst of reads separates the two: a node held by
	 * the excitation resistor and the sensor barely moves. */
	if (!ret && drv->max_spread_mv) {
		int lo = mv;
		int hi = mv;

		for (int n = 0; n < APP_SENSOR_STABILITY_READS; n++) {
			uint16_t r;

			k_msleep(APP_SENSOR_STABILITY_GAP_MS);

			if (ctr_adc_read(app_x0_adc_map[slot->ch_measure - 1], &r)) {
				continue;
			}

			int v = (int)CTR_ADC_X0_AI_MILLIVOLTS(r);

			lo = MIN(lo, v);
			hi = MAX(hi, v);
		}

		if (hi - lo > drv->max_spread_mv) {
			LOG_WRN("sensor-%d: ch%u wandered %d mV (%d..%d) in %d ms — nothing is "
				"holding this node, so it is not a measurement; check the "
				"wiring and run `sensor scan`",
				i + 1, slot->ch_measure, hi - lo, lo, hi,
				APP_SENSOR_STABILITY_READS * APP_SENSOR_STABILITY_GAP_MS);
			ret = -EIO;
		}
	}

	/* Never hand an impossible number to the driver: with a ×11 error the value
	 * lands above any flood threshold and would be reported as a confident flood. */
	if (!ret && mv > APP_SENSOR_MAX_PLAUSIBLE_MV) {
		LOG_ERR("sensor-%d: ch%u still reads %d mV after %d attempts — the input "
			"divider will not engage; discarding the sample",
			i + 1, slot->ch_measure, mv, APP_SENSOR_DIVIDER_RETRIES + 1);
		ret = -EIO;
	}

	/* Tear down the excitation only. The measure channel is left in AI_INPUT: with
	 * the supply off there is nothing to drive current through the divider, and the
	 * next sample re-drives the pins anyway. */
	if (drv->needs_power) {
		/* Checked, unlike a cosmetic teardown: if this fails the excitation rail
		 * stays on, which both drains the battery continuously and leaves the
		 * sensor permanently energised. That must not pass silently. */
		int pret = ctr_x0_set_mode(pdev, pch, CTR_X0_MODE_DEFAULT);

		if (pret) {
			LOG_ERR("sensor-%d: cannot switch ch%u back off (%d) — the excitation "
				"rail is still powered",
				i + 1, slot->ch_power, pret);
		}
	}

	if (ret) {
		if (ret != -EIO) {
			LOG_ERR("sensor-%d: `ctr_adc_read` failed: %d", i + 1, ret);
		}
		ret = sample_discard(drv, i, ret);
		k_mutex_unlock(&m_sample_lock);
		return ret;
	}

	/* Propagates APP_SENSOR_SAMPLE_ALARM to the caller, which dispatches the
	 * resulting report outside this lock. */
	ret = drv->sample(i, mv);

	k_mutex_unlock(&m_sample_lock);

	return ret;
}

/* ------------------------------------------------------------------------- */

/* `sensor scan` — identify which X0 channel a sensor is actually wired to.
 *
 * The terminal labels on an expansion carrier do not have to match the X0
 * channel numbers the firmware addresses, and getting the pair wrong is silent:
 * the configured channel simply reads whatever floats on an unconnected pin,
 * which looks like a working measurement. Diagnosing that from the outside took
 * days, so it is a command rather than folklore.
 *
 * Every channel has to be read in one pass, with the excitation on and all the
 * dividers engaged. Stepping through the channels one at a time cannot identify
 * anything: the node moves while the excitation settles, so readings taken
 * seconds apart are not comparable either to each other or to a multimeter.
 */
#define APP_SENSOR_SCAN_SETTLE_MS 1500
#define APP_SENSOR_SCAN_PASSES    5

/* A live sensor node sits inside the LD-81 driver's fail-safe window: at or below
 * LD81_SHORT_CIRCUIT_MV the pin is grounded or unconnected; at or above
 * LD81_OPEN_CIRCUIT_MV it is pulled up to the excitation rail with nothing loading
 * it, i.e. an open circuit. The scan judges with the driver's own limits, so a
 * channel it calls live is one the driver will accept.
 *
 * A level alone is not enough to tell a sensor from an unconnected pin: an open
 * input picks up noise and lands anywhere, including squarely inside the window.
 * What separates them is stability — a node held by an excitation resistor and a
 * sensor barely moves between passes, while a floating pin wanders by a volt or
 * more. Measured on the bench: the real node spread 77 mV across five passes, the
 * unconnected channel next to it spread 979 mV. The driver's `max_spread_mv` is the
 * limit for that. */

/* Which configured slot a scan should be judged against. An explicit `slot_arg`
 * (1-based) always wins. Otherwise, when the caller named the supply channel, only a
 * sensor actually on that channel counts: judging a scan of ch4 against sensor-1 on ch2
 * would name the wrong slot in the mapping hint and warn that sensor-1 is "reading an
 * unconnected pin" about a channel the scan never drove. With no channel named there is
 * nothing to match on and nothing else to derive the excitation from, so the first
 * configured slot is taken and the scan runs on its supply channel. Returns NULL when
 * there is no configured sensor to compare against; `*out_slot` is still set for an
 * explicitly named slot, so the caller can name it in the hint without comparing. */
static struct app_data_sensor *scan_slot(const struct shell *shell, int ch_power,
					 int slot_arg, int *out_slot)
{
	*out_slot = -1;

	if (slot_arg >= 1 && slot_arg <= APP_SENSORS_MAX) {
		struct app_data_sensor *s = &g_app_data.sensors[slot_arg - 1];

		/* Reported even when the slot holds nothing: naming an unconfigured slot is
		 * the normal way to use this — "which channel is the detector I am about to
		 * make sensor-3 on?" — so the slot still steers the closing hint. Only the
		 * comparison needs a configured sensor, and that is gated on the return
		 * value being non-NULL. */
		*out_slot = slot_arg;

		return s->configured ? s : NULL;
	}

	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		if (!g_app_data.sensors[i].configured) {
			continue;
		}
		if (ch_power >= 1 && g_app_data.sensors[i].ch_power != ch_power) {
			continue;
		}

		*out_slot = i + 1;

		return &g_app_data.sensors[i];
	}

	return NULL;
}

/* True for a channel some configured sensor measures on. Those are left in AI_INPUT by
 * the restore loop: the sensor layer holds them there so the input divider stays engaged,
 * and parking one to DEFAULT disengages it until that slot next samples. Checked across
 * every slot, not just the one being judged, so scanning for sensor 3 does not disturb
 * sensors 1, 2 and 4. */
static bool scan_is_measure_ch(int ch)
{
	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		if (g_app_data.sensors[i].configured && g_app_data.sensors[i].ch_measure == ch) {
			return true;
		}
	}

	return false;
}

/* The slot the closing "wire it up as" hint should name: the one the verdict was judged
 * against; failing that the first free slot, so the hint does not tell the operator to
 * overwrite a sensor that is already configured; failing that slot 1. */
static int scan_hint_slot(int slot_num)
{
	if (slot_num >= 1) {
		return slot_num;
	}

	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		if (!g_app_data.sensors[i].configured) {
			return i + 1;
		}
	}

	return 1;
}

int app_sensor_drv_scan(const struct shell *shell, int ch_power, int slot_arg)
{
	int slot_num;
	struct app_data_sensor *slot = scan_slot(shell, ch_power, slot_arg, &slot_num);
	int totals[APP_DATA_NUM_CHANNELS] = {0};
	int counts[APP_DATA_NUM_CHANNELS] = {0};
	int mins[APP_DATA_NUM_CHANNELS];
	int maxs[APP_DATA_NUM_CHANNELS];
	const struct device *pdev;
	enum ctr_x0_channel pch;

	/* Default to the configured sensor's supply channel, so the common case is
	 * a bare `sensor scan`. Without a sensor configured there is nothing to
	 * infer it from and the caller has to say. */
	if (ch_power < 0) {
		if (slot == NULL) {
			shell_error(shell, "no sensor configured — say which channel supplies "
					   "the excitation: sensor scan <1-%d> [slot]",
				    APP_DATA_NUM_CHANNELS);
			return -EINVAL;
		}
		ch_power = slot->ch_power;
	}

	if (ch_power < 1 || ch_power > APP_DATA_NUM_CHANNELS) {
		shell_error(shell, "invalid supply channel: %d (1-%d)", ch_power,
			    APP_DATA_NUM_CHANNELS);
		return -EINVAL;
	}

	/* Refuse rather than drive a channel that is in use. Driving the supply channel
	 * means taking it over and pushing the excitation into whatever is connected. */
	if (!ch_scan_may_supply(ch_power)) {
		shell_error(shell,
			    "ch%d is in use (channel-mode-%d is not `disabled` or `power`); "
			    "scanning would drive the excitation into whatever is connected "
			    "there. Name a disabled channel, or set channel-mode-%d to disabled "
			    "first",
			    ch_power, ch_power, ch_power);
		return -EBUSY;
	}

	/* Say which sensor the verdict below is judged against, now that the scan is
	 * definitely going ahead — nothing above this point has committed to scanning.
	 * Only an explicitly named slot can be off the channel being driven; say so,
	 * because its measure channel is then unrelated to what this scan sees. */
	if (slot == NULL && slot_num >= 1) {
		shell_warn(shell, "sensor-%d is not configured; scanning without a channel to "
				  "compare against",
			   slot_num);
	}
	if (slot != NULL) {
		shell_print(shell, "judging against sensor-%d (power ch%u, measure ch%u)%s",
			    slot_num, slot->ch_power, slot->ch_measure,
			    slot->ch_power == ch_power
				    ? ""
				    : " — note it is not on the channel being driven");
	}

	pdev = ch_device(ch_power);
	pch = app_x0_channel_map[ch_power - 1];

	/* Serialized against the sample path: this drives the same channels. */
	k_mutex_lock(&m_sample_lock, K_FOREVER);

	int ret = ctr_x0_set_mode(pdev, pch, CTR_X0_MODE_PWR_SOURCE);

	if (ret) {
		shell_error(shell, "cannot power ch%d (X0 module not ready?): %d", ch_power, ret);
		k_mutex_unlock(&m_sample_lock);
		return ret;
	}

	shell_print(shell, "excitation on ch%d; reading every other channel as analog input",
		    ch_power);

	/* Skip a channel whose ADC input `ctr_adc_init` rejects, so the read loop does
	 * not spam an error per pass per dead channel. (A3 was rejected before the
	 * ctr_adc input mapping fix a317b690, which mapped it to a nonexistent AIN.) */
	bool ready[APP_DATA_NUM_CHANNELS] = {false};

	for (int ch = 1; ch <= APP_DATA_NUM_CHANNELS; ch++) {
		if (ch == ch_power) {
			continue;
		}
		if (ch_scan_skips(ch - 1)) {
			shell_print(shell,
				    "  ch%d: trigger, counter or current input — left untouched",
				    ch);
			continue;
		}
		if (ctr_adc_init(app_x0_adc_map[ch - 1])) {
			shell_warn(shell, "ch%d: this channel's ADC input is unavailable on "
					  "this board; skipping", ch);
			continue;
		}
		ready[ch - 1] = true;
		ctr_x0_set_mode(ch_device(ch), app_x0_channel_map[ch - 1], CTR_X0_MODE_AI_INPUT);
	}

	k_msleep(APP_SENSOR_SCAN_SETTLE_MS);

	for (int pass = 0; pass < APP_SENSOR_SCAN_PASSES; pass++) {
		char line[96];
		int pos = 0;

		for (int ch = 1; ch <= APP_DATA_NUM_CHANNELS; ch++) {
			uint16_t raw;

			if (ch == ch_power) {
				pos += snprintf(line + pos, sizeof(line) - pos, " %8s", "supply");
				continue;
			}
			if (!ready[ch - 1]) {
				pos += snprintf(line + pos, sizeof(line) - pos, " %8s", "n/a");
				continue;
			}
			if (ctr_adc_read(app_x0_adc_map[ch - 1], &raw)) {
				pos += snprintf(line + pos, sizeof(line) - pos, " %8s", "-");
				continue;
			}

			int mv = (int)CTR_ADC_X0_AI_MILLIVOLTS(raw);

			if (!counts[ch - 1]) {
				mins[ch - 1] = mv;
				maxs[ch - 1] = mv;
			} else if (mv < mins[ch - 1]) {
				mins[ch - 1] = mv;
			} else if (mv > maxs[ch - 1]) {
				maxs[ch - 1] = mv;
			}

			totals[ch - 1] += mv;
			counts[ch - 1]++;
			pos += snprintf(line + pos, sizeof(line) - pos, " %8d", mv);
		}
		shell_print(shell, "  ch:%s", line);
		k_msleep(200);
	}

	/* Put the hardware back the way the application had it, not to a blanket
	 * DEFAULT: a channel configured as `voltage` is held in AI_INPUT permanently by
	 * init, and parking it would leave the app in a state it never set up. */
	/* The supply channel too. Only a sensor supply or a `disabled` / `power` channel
	 * gets this far, and those are all held in DEFAULT. */
	ctr_x0_set_mode(pdev, pch, ch_held_mode(ch_power));

	for (int ch = 1; ch <= APP_DATA_NUM_CHANNELS; ch++) {
		/* Only channels this command actually drove. `ready` excludes the inputs it
		 * deliberately skipped, so they keep their NPN/PNP or CL_INPUT state. */
		if (!ready[ch - 1] || scan_is_measure_ch(ch)) {
			continue;
		}

		ctr_x0_set_mode(ch_device(ch), app_x0_channel_map[ch - 1], ch_held_mode(ch));
	}

	k_mutex_unlock(&m_sample_lock);

	int found = 0;
	int found_ch = 0;

	shell_print(shell, "");

	for (int ch = 1; ch <= APP_DATA_NUM_CHANNELS; ch++) {
		if (ch == ch_power || !counts[ch - 1]) {
			continue;
		}

		int mv = totals[ch - 1] / counts[ch - 1];
		int spread = maxs[ch - 1] - mins[ch - 1];
		const char *verdict;

		if (mv <= LD81_SHORT_CIRCUIT_MV) {
			verdict = "grounded or nothing connected";
		} else if (mv >= LD81_OPEN_CIRCUIT_MV) {
			verdict = "pulled to the supply rail — open circuit";
		} else if (spread > ld81_driver.max_spread_mv) {
			verdict = "drifting — an unconnected pin picking up noise";
		} else {
			verdict = "*** live sensor node ***";
			found++;
			found_ch = ch;
		}

		shell_print(shell, "  channel %d: %5d mV  (spread %d mV)  %s", ch, mv, spread,
			    verdict);
	}

	shell_print(shell, "");

	if (found == 1) {
		shell_print(shell, "wire it up as:  app config sensor-%d \"ld-81,%d,%d\"",
			    scan_hint_slot(slot_num), ch_power, found_ch);
		/* Only meaningful for a sensor on the channel just driven — otherwise its
		 * measure channel has nothing to do with what this scan measured. */
		if (slot != NULL && slot->ch_power == ch_power && slot->ch_measure != found_ch) {
			shell_warn(shell,
				   "the configured measure channel is ch%u, but the sensor is on "
				   "ch%d — ch%u is reading an unconnected pin",
				   slot->ch_measure, found_ch, slot->ch_measure);
		}
	} else if (found == 0) {
		shell_warn(shell,
			   "no channel looks like a live sensor. Check the excitation resistor, "
			   "the sensor GND, and that ch%d really is the supply terminal",
			   ch_power);
	} else {
		shell_warn(shell, "%d channels look live — disconnect the sensor and run this "
				  "again; the channel that changes is the one it is on",
			   found);
	}

	return 0;
}

int app_sensor_drv_sample_all(void)
{
	bool alarm = false;

	for (int i = 0; i < APP_SENSORS_MAX; i++) {
		if (!g_app_data.sensors[i].configured) {
			continue;
		}
		if (app_sensor_drv_sample_slot(i) == APP_SENSOR_SAMPLE_ALARM) {
			alarm = true;
		}
	}

	/* One report covers every slot that changed in this sweep. */
	if (alarm) {
		LOG_INF("Sensor state changed; requesting a report");
		app_handler_report_event();
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
