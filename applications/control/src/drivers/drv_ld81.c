/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "drv_ld81.h"
#include "../app_config.h"
#include "../feature.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_ld81, LOG_LEVEL_INF);

/* Everything below reaches into config that only exists on an X0_A build
 * (APP_SENSORS_MAX, `g_app_config.ld81_threshold_mv`), and CMakeLists compiles this
 * file unconditionally — so without this guard a Control build for a board with no
 * CHESTER-X0-A fails to compile. Matches the guard in app_sensor_drv.c. */
#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

/* Jablotron LD-81 — 2-wire conductivity flood detector.
 *
 * Wiring on CHESTER-X0-A: the power channel sources a switched 5 V rail through
 * an external ~8k2 pull-up to the measurement channel; the sensor OUT joins the
 * measurement node, sensor GND to GND. The generic runtime (app_sensor_drv.c)
 * pulses the power channel, settles, reads the measurement channel as mV, then
 * powers everything back down.
 *
 * Calibrated on hardware 2026-07-21: dry ~770 mV, flooded ~1690 mV. The LD-81
 * RAISES the sense voltage when flooded, so flood = voltage ABOVE threshold.
 *
 * The threshold is runtime-configurable via `app config sensor-ld81-threshold <mV>`
 * (default 1100 mV, set in app_config.c).
 */
#define LD81_FLOODED_ABOVE true

/* The fail-safe window, the hysteresis and the fault confirmation are defined in
 * drv_ld81.h, so the config range, `sensor scan` and the host tests use the same values. */

/* Private per-slot decoded data. The driver is the sole writer; consumers read
 * via ld81_get_data(). */
static struct app_data_ld81 m_data[APP_SENSORS_MAX];

/* Level of the last valid reading. It is kept through a fault, so the first reading
 * after one is compared with what was last reported: a sensor that comes back in the
 * same state raises no alarm, one that comes back changed does. The hysteresis is
 * measured against it for the same reason. */
enum ld81_level {
	LD81_LEVEL_UNKNOWN = 0, /* no valid reading yet */
	LD81_LEVEL_DRY,
	LD81_LEVEL_FLOODED,
};

/* Alarm bookkeeping per slot, next to the data in `m_data`. */
struct ld81_track {
	enum ld81_level level;
	/* A fault is confirmed and no valid reading has followed it yet. */
	bool fault;
	/* Faulty samples in a row, up to LD81_FAULT_CONFIRM_SAMPLES. */
	uint8_t faults;
	/* A fault alarm was raised, at uptime `fault_alarm_ms`; see
	 * LD81_FAULT_ALARM_HOLDOFF_S. */
	bool fault_alarmed;
	int64_t fault_alarm_ms;
};

static struct ld81_track m_track[APP_SENSORS_MAX];

static K_MUTEX_DEFINE(m_lock);

static int ld81_init(int slot, uint8_t ch_power, uint8_t ch_measure)
{
	LOG_INF("LD-81 init: slot %d, power ch%u, measure ch%u", slot + 1, ch_power, ch_measure);

	/* The threshold has to sit inside the fail-safe window, or it can never be
	 * crossed: the two guards in ld81_sample() fire first and the slot is marked
	 * faulty instead. `app config sensor-ld81-threshold` refuses such a value, but one
	 * stored by an older build (which accepted 0..10000 mV) still loads, so say so
	 * rather than letting flood detection quietly never trigger. */
	if (g_app_config.ld81_threshold_mv < LD81_THRESHOLD_MIN_MV ||
	    g_app_config.ld81_threshold_mv > LD81_THRESHOLD_MAX_MV) {
		LOG_WRN("LD-81: threshold %d mV is outside the usable %d..%d mV range — a "
			"flood may never be detected, or a flooded sensor never read dry again; "
			"`app config sensor-ld81-threshold` it back (default 1100)",
			g_app_config.ld81_threshold_mv, LD81_THRESHOLD_MIN_MV,
			LD81_THRESHOLD_MAX_MV);
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	memset(&m_data[slot], 0, sizeof(m_data[slot])); /* valid = false */
	memset(&m_track[slot], 0, sizeof(m_track[slot])); /* level unknown, no fault */
	k_mutex_unlock(&m_lock);

	return 0;
}

/* Mark a slot faulty. The data is invalidated at once, so no consumer ever sees a
 * reading from a faulty sample. Losing a working sensor is itself worth reporting now:
 * a slot that had valid data and stops having it means the detector has gone away,
 * and on a 30-minute report interval that would otherwise not be visible for half
 * an hour — and a FLOODED slot going invalid looks exactly like silence.
 *
 * A terminal that is only loose must not turn that into a stream of alarms, though:
 * they all share the event rate limit with the triggers and the power events. So the
 * report waits for LD81_FAULT_CONFIRM_SAMPLES faulty samples in a row, a slot raises
 * at most one fault alarm per LD81_FAULT_ALARM_HOLDOFF_S, and a valid reading in
 * between alarms only when its level differs from the one before the fault.
 *
 * The data and the state change in one critical section, so a concurrent reader
 * can never observe the in-between state. */
static int ld81_fault(int slot)
{
	struct ld81_track *t = &m_track[slot];
	bool confirmed = false;
	bool alarm = false;

	k_mutex_lock(&m_lock, K_FOREVER);
	m_data[slot].valid = false;

	if (t->faults < LD81_FAULT_CONFIRM_SAMPLES) {
		t->faults++;
	}

	if (t->faults >= LD81_FAULT_CONFIRM_SAMPLES && !t->fault) {
		int64_t now = k_uptime_get();

		t->fault = true;
		confirmed = true;

		/* Only a slot that had a valid reading has something to report losing. */
		alarm = t->level != LD81_LEVEL_UNKNOWN &&
			(!t->fault_alarmed ||
			 now - t->fault_alarm_ms >= LD81_FAULT_ALARM_HOLDOFF_S * MSEC_PER_SEC);

		if (alarm) {
			t->fault_alarmed = true;
			t->fault_alarm_ms = now;
		}
	}
	k_mutex_unlock(&m_lock);

	if (confirmed) {
		LOG_WRN("LD-81: slot %d, fault confirmed after %d samples%s", slot + 1,
			LD81_FAULT_CONFIRM_SAMPLES,
			alarm ? "" : " (no report: no reading yet, or one sent recently)");
	}

	return alarm ? APP_SENSOR_SAMPLE_ALARM : 0;
}

static int ld81_sample(int slot, int raw_mv)
{
	int threshold_mv = g_app_config.ld81_threshold_mv;

	/* Fail safe before deciding anything: an open circuit sits above the flood
	 * threshold, so without this every broken cable reports a flood. */
	if (raw_mv >= LD81_OPEN_CIRCUIT_MV) {
		LOG_WRN("LD-81: slot %d, %d mV >= %d mV — open circuit, not a flood; "
			"check the sensor wiring and terminals, and run `sensor scan` to see "
			"which channel it is actually on",
			slot + 1, raw_mv, LD81_OPEN_CIRCUIT_MV);
		return ld81_fault(slot);
	}

	/* And the other way: the node cannot be pulled to ~0 V while a sensor is
	 * connected, so this is a short or a dead ADC, not a dry sensor. Reporting it
	 * as `dry` would leave the alarm silently disabled. */
	if (raw_mv <= LD81_SHORT_CIRCUIT_MV) {
		LOG_WRN("LD-81: slot %d, %d mV <= %d mV — the sense line looks shorted to "
			"GND, not dry; the flood alarm cannot work in this state",
			slot + 1, raw_mv, LD81_SHORT_CIRCUIT_MV);
		return ld81_fault(slot);
	}

	struct ld81_track *t = &m_track[slot];

	k_mutex_lock(&m_lock, K_FOREVER);

	/* Hysteresis: a flooded slot turns dry only once the reading is a band past the
	 * threshold, so a sensor sitting at the threshold does not flip on every sample. */
	bool was_flooded = t->level == LD81_LEVEL_FLOODED;
	int band_mv = was_flooded ? LD81_HYSTERESIS_MV : 0;
	bool flooded = LD81_FLOODED_ABOVE ? (raw_mv > threshold_mv - band_mv)
					  : (raw_mv < threshold_mv + band_mv);
	enum ld81_level level = flooded ? LD81_LEVEL_FLOODED : LD81_LEVEL_DRY;

	/* Flooding is an alarm, not a slow telemetry value: report a change in the
	 * flooded state immediately instead of waiting for the next periodic
	 * report, which could be a full report interval away (and would miss a
	 * flood that receded in between). This holds across a fault too: the level
	 * before it is the reference, so a flood that cleared while the sensor was
	 * faulty is reported. A first valid reading counts as a change only when it
	 * is already flooded — dry is the normal state and must not trigger a report. */
	bool alarm = level != t->level && (flooded || was_flooded);

	t->level = level;
	t->fault = false;
	t->faults = 0;
	m_data[slot].voltage_mv = raw_mv;
	m_data[slot].flooded = flooded;
	m_data[slot].valid = true;
	k_mutex_unlock(&m_lock);

	LOG_INF("LD-81: slot %d, %d mV (threshold %d mV%s) -> %s%s", slot + 1, raw_mv, threshold_mv,
		flooded ? ", minus the hysteresis band to clear" : "", flooded ? "FLOODED" : "dry",
		alarm ? " (changed)" : "");

	return alarm ? APP_SENSOR_SAMPLE_ALARM : 0;
}

/* A sample the runtime had to discard counts as a faulty sample, and is reported
 * the same way as an open or shorted sense line. */
static int ld81_invalidate(int slot)
{
	return ld81_fault(slot);
}

static void ld81_print_data(const struct shell *shell, int slot)
{
	k_mutex_lock(&m_lock, K_FOREVER);
	struct app_data_ld81 d = m_data[slot];
	k_mutex_unlock(&m_lock);

	if (!d.valid) {
		shell_print(shell, "[%d] LD-81: no valid sample", slot + 1);
		return;
	}
	shell_print(shell, "[%d] LD-81: %d mV, flooded: %s", slot + 1, d.voltage_mv,
		    d.flooded ? "yes" : "no");
}

const struct app_sensor_driver ld81_driver = {
	.name = "ld-81",
	.type_id = APP_SENSOR_TYPE_LD81,
	.needs_power = true,
	/* 100 ms is the original justified settle: the LD-81 divider settles in well
	 * under that, verified on hardware after the X0 input-positive fix (dry 759 mV /
	 * flooded 1724 mV, matching a meter). The earlier 1500 ms was measured on a rig
	 * where the reading was actually a floating pin (the X0 input off-by-one, fixed
	 * in ctr_adc.c / chester/sdk!865), so it was never justified — and it powered
	 * the excitation 15x longer than needed on every sample, wasting battery. */
	.power_on_delay_ms = 100,
	/* Measured on hardware: the real node moved 77 mV across five reads while an
	 * unconnected channel on the same module moved 957 mV. 250 mV sits well clear of
	 * both, and no flood appears or recedes inside the ~120 ms burst. */
	.max_spread_mv = 250,
	.init = ld81_init,
	.sample = ld81_sample,
	.invalidate = ld81_invalidate,
	.print_data = ld81_print_data,
};

int ld81_get_data(int slot, struct app_data_ld81 *out)
{
	if (slot < 0 || slot >= APP_SENSORS_MAX || !out) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	*out = m_data[slot];
	k_mutex_unlock(&m_lock);

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */
