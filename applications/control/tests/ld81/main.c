/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* LD-81 flood detector simulator.
 *
 * The LD-81 is a passive conductivity sensor: the driver only ever sees a
 * millivolt reading from the X0 analog input. That makes it fully simulatable on
 * the host — feed `sample()` a sequence of millivolt values and assert the
 * decoded state and the alarm decisions, with no hardware, no X0 module and no
 * water.
 *
 * Run: west build -b native_sim/native/64 && ./build/zephyr/zephyr.exe
 */

#include "drivers/drv_ld81.h"
#include "app_config.h"

#include <zephyr/ztest.h>

/* The driver's only external dependency: the runtime-configurable levels. */
struct app_config g_app_config;

/* The driver's print_data() is a shell helper and is not under test. Stub the
 * one symbol it pulls in, rather than linking the whole shell subsystem into a
 * logic test. */
void shell_fprintf_normal(const struct shell *sh, const char *fmt, ...)
{
	ARG_UNUSED(sh);
	ARG_UNUSED(fmt);
}

#define SLOT 0

/* Bench calibration from 2026-07-21 (CHESTER-M + X0-A, real LD-81). */
#define DRY_MV       770
#define FLOODED_MV   1690
#define THRESHOLD_MV 1100

/* The fail-safe window and the hysteresis come from drv_ld81.h, so the tests assert
 * against the values the driver uses. */
#define OPEN_MV   LD81_OPEN_CIRCUIT_MV
#define SHORT_MV  LD81_SHORT_CIRCUIT_MV
#define DRY_AT_MV (THRESHOLD_MV - LD81_HYSTERESIS_MV)

/* ld81_get_data() snapshots under the driver's lock rather than handing out a
 * pointer, so every assertion has to re-read after the sample it is about. */
static struct app_data_ld81 snap(void)
{
	struct app_data_ld81 d;

	zassert_ok(ld81_get_data(SLOT, &d), "getter rejected a valid slot");

	return d;
}

static void reset_slot(void)
{
	g_app_config.ld81_threshold_mv = THRESHOLD_MV;
	zassert_ok(ld81_driver.init(SLOT, 2, 1), "init failed");
}

static int sample(int mv)
{
	return ld81_driver.sample(SLOT, mv);
}

ZTEST_SUITE(ld81, NULL, NULL, NULL, NULL, NULL);

/* A fresh slot has no reading yet, so nothing may be reported as valid. */
ZTEST(ld81, test_starts_invalid)
{
	reset_slot();

	zassert_false(snap().valid, "a slot with no sample must not be valid");
}

/* Threshold comparison, using the measured bench values. */
ZTEST(ld81, test_threshold)
{
	reset_slot();

	sample(DRY_MV);
	struct app_data_ld81 d = snap();

	zassert_true(d.valid);
	zassert_equal(d.voltage_mv, DRY_MV, "voltage must be stored verbatim in mV");
	zassert_false(d.flooded, "%d mV is below the %d mV threshold -> dry", DRY_MV,
		      THRESHOLD_MV);

	/* Exactly at the threshold is NOT flooded (flood = strictly above). */
	sample(THRESHOLD_MV);
	zassert_false(snap().flooded, "exactly at the threshold must not count as flooded");

	sample(FLOODED_MV);
	zassert_true(snap().flooded, "%d mV is above the %d mV threshold -> flooded", FLOODED_MV,
		     THRESHOLD_MV);
}

/* Once flooded, the slot turns dry only at or below the hysteresis band under the
 * threshold, so a reading that hovers at the threshold cannot raise an alarm on every
 * sample. */
ZTEST(ld81, test_hysteresis)
{
	reset_slot();

	zassert_equal(sample(FLOODED_MV), APP_SENSOR_SAMPLE_ALARM);

	zassert_equal(sample(THRESHOLD_MV), 0, "at the threshold a flooded slot stays flooded");
	zassert_true(snap().flooded);
	zassert_equal(sample(DRY_AT_MV + 1), 0, "inside the band a flooded slot stays flooded");
	zassert_true(snap().flooded);

	zassert_equal(sample(DRY_AT_MV), APP_SENSOR_SAMPLE_ALARM,
		      "at the bottom of the band the slot turns dry");
	zassert_false(snap().flooded);

	/* Back from dry, the threshold itself applies again. */
	zassert_equal(sample(THRESHOLD_MV), 0, "dry at the threshold stays dry");
	zassert_equal(sample(THRESHOLD_MV + 1), APP_SENSOR_SAMPLE_ALARM);
}

/* A reading that wobbles around the threshold raises one alarm, not one per sample. */
ZTEST(ld81, test_hovering_at_the_threshold_alarms_once)
{
	int alarms = 0;

	reset_slot();
	sample(DRY_MV);

	for (int i = 0; i < 10; i++) {
		alarms += sample(i % 2 ? THRESHOLD_MV - 40 : THRESHOLD_MV + 40) ==
			  APP_SENSOR_SAMPLE_ALARM;
	}

	zassert_equal(alarms, 1, "a reading hovering at the threshold alarmed %d times", alarms);
}

/* The alarm contract: a report is requested only when the state CHANGES, so a
 * steady sensor stays quiet and a transition is never missed. */
ZTEST(ld81, test_alarm_on_change_only)
{
	reset_slot();

	zassert_equal(sample(DRY_MV), 0, "booting dry must not raise an alarm");
	zassert_equal(sample(DRY_MV), 0, "still dry must not raise an alarm");

	zassert_equal(sample(FLOODED_MV), APP_SENSOR_SAMPLE_ALARM,
		      "dry -> flooded must raise an alarm");
	zassert_equal(sample(FLOODED_MV), 0, "still flooded must not re-alarm");
	zassert_equal(sample(FLOODED_MV), 0, "still flooded must not re-alarm");

	zassert_equal(sample(DRY_MV), APP_SENSOR_SAMPLE_ALARM,
		      "flooded -> dry must raise an alarm (the flood cleared)");
	zassert_equal(sample(DRY_MV), 0, "dry again must not re-alarm");
}

/* A device that boots with water already present must report immediately —
 * otherwise an existing flood stays invisible until it happens to change. */
ZTEST(ld81, test_alarm_when_first_reading_is_flooded)
{
	reset_slot();

	zassert_equal(sample(FLOODED_MV), APP_SENSOR_SAMPLE_ALARM,
		      "first reading already flooded must raise an alarm");
}

/* One discarded sample clears the data at once but does not change the state the
 * alarms are raised against, so the same reading after it is not a change. */
ZTEST(ld81, test_single_invalidate_does_not_realarm)
{
	reset_slot();

	sample(FLOODED_MV);
	zassert_equal(ld81_driver.invalidate(SLOT), 0, "one lost sample must not alarm");
	zassert_false(snap().valid, "invalidate must clear the valid flag");

	zassert_equal(sample(FLOODED_MV), 0, "still flooded after one lost sample is no change");
	zassert_true(snap().valid);
}

static void confirm_fault(void)
{
	for (int i = 0; i < LD81_FAULT_CONFIRM_SAMPLES; i++) {
		(void)ld81_driver.invalidate(SLOT);
	}
}

/* A confirmed fault keeps the level of the last valid reading: the sensor coming back
 * in the same state is no change, coming back in another state is. */
ZTEST(ld81, test_fault_keeps_the_level)
{
	reset_slot();
	sample(FLOODED_MV);
	confirm_fault();
	zassert_equal(sample(FLOODED_MV), 0, "back flooded after a fault is no change");

	reset_slot();
	sample(DRY_MV);
	confirm_fault();
	zassert_equal(sample(DRY_MV), 0, "back dry after a fault is no change");

	reset_slot();
	sample(DRY_MV);
	confirm_fault();
	zassert_equal(sample(FLOODED_MV), APP_SENSOR_SAMPLE_ALARM,
		      "flooded after a fault on a dry sensor must alarm");

	/* No valid reading before the fault: like a first reading, flooded alarms. */
	reset_slot();
	confirm_fault();
	zassert_equal(sample(FLOODED_MV), APP_SENSOR_SAMPLE_ALARM);
	reset_slot();
	confirm_fault();
	zassert_equal(sample(DRY_MV), 0);
}

/* A flood that cleared while the sensor was faulty is reported when it comes back. */
ZTEST(ld81, test_flood_cleared_during_a_fault_alarms)
{
	reset_slot();
	sample(FLOODED_MV);
	confirm_fault();

	zassert_equal(sample(DRY_MV), APP_SENSOR_SAMPLE_ALARM,
		      "dry after a fault on a flooded sensor must alarm");
	zassert_false(snap().flooded);
}

/* The hysteresis band holds across a fault, confirmed or not. */
ZTEST(ld81, test_hysteresis_survives_a_fault)
{
	reset_slot();
	sample(FLOODED_MV);
	(void)ld81_driver.invalidate(SLOT);
	zassert_equal(sample(THRESHOLD_MV - 50), 0, "inside the band after one lost sample");
	zassert_true(snap().flooded);

	confirm_fault();
	zassert_equal(sample(THRESHOLD_MV - 50), 0, "inside the band after a confirmed fault");
	zassert_true(snap().flooded);
}

/* A sample the runtime discards (floating input, divider, X0 not responding, ADC
 * error) is a fault like an open circuit: a flooded sensor whose wire comes loose is
 * reported once the fault is confirmed, not left quiet until the next periodic report. */
ZTEST(ld81, test_discarded_samples_alarm_like_a_fault)
{
	reset_slot();

	sample(FLOODED_MV);

	for (int i = 1; i < LD81_FAULT_CONFIRM_SAMPLES; i++) {
		zassert_equal(ld81_driver.invalidate(SLOT), 0,
			      "an unconfirmed fault must not alarm");
	}

	zassert_equal(ld81_driver.invalidate(SLOT), APP_SENSOR_SAMPLE_ALARM,
		      "a confirmed fault on a working sensor must alarm");
	zassert_equal(ld81_driver.invalidate(SLOT), 0, "a lasting fault must not re-alarm");

	/* No reading yet, so nothing to report losing. */
	reset_slot();
	for (int i = 0; i <= LD81_FAULT_CONFIRM_SAMPLES; i++) {
		zassert_equal(ld81_driver.invalidate(SLOT), 0);
	}
}

/* `app config sensor-ld81-threshold` must take effect without a rebuild. */
ZTEST(ld81, test_threshold_is_runtime_configurable)
{
	reset_slot();

	sample(1000);
	zassert_false(snap().flooded, "1000 mV is dry at a 1100 mV threshold");

	g_app_config.ld81_threshold_mv = 900;
	zassert_equal(sample(1000), APP_SENSOR_SAMPLE_ALARM,
		      "lowering the threshold below the reading must flood and alarm");
	zassert_true(snap().flooded);
}

/* Slots are independent: one sensor's state must not leak into another's. */
ZTEST(ld81, test_slots_are_independent)
{
	struct app_data_ld81 a, b;

	g_app_config.ld81_threshold_mv = THRESHOLD_MV;
	zassert_ok(ld81_driver.init(0, 2, 1));
	zassert_ok(ld81_driver.init(1, 4, 3));

	zassert_equal(ld81_driver.sample(0, FLOODED_MV), APP_SENSOR_SAMPLE_ALARM);
	zassert_equal(ld81_driver.sample(1, DRY_MV), 0);

	zassert_ok(ld81_get_data(0, &a));
	zassert_ok(ld81_get_data(1, &b));
	zassert_true(a.flooded, "slot 0 is flooded");
	zassert_false(b.flooded, "slot 1 is dry");
}

/* Out-of-range slots and a NULL destination must be rejected rather than read or
 * written out of bounds. */
ZTEST(ld81, test_getter_rejects_bad_slot)
{
	struct app_data_ld81 d;

	zassert_equal(ld81_get_data(-1, &d), -EINVAL);
	zassert_equal(ld81_get_data(APP_SENSORS_MAX, &d), -EINVAL);
	zassert_equal(ld81_get_data(SLOT, NULL), -EINVAL);
}

/* ------------------------------------------------------------------------- */
/* Fail-safe window. These are the cases that decide whether a broken sensor is
 * noticed or silently reported as a normal reading. */

/* A node pulled up towards the excitation rail is a broken cable, not a flood.
 * Reporting it as FLOODED was the original bug. */
ZTEST(ld81, test_open_circuit_is_a_fault_not_a_flood)
{
	reset_slot();

	zassert_equal(sample(OPEN_MV), 0, "no prior valid reading, so nothing to re-report");

	struct app_data_ld81 d = snap();

	zassert_false(d.valid, "an open circuit must invalidate the slot, not report a flood");
	zassert_false(d.flooded, "an open circuit is not a flood");
}

/* The floor, and the one that matters: a shorted sense line or a dead ADC reads
 * near zero, which is below the threshold and would otherwise pass as `dry` —
 * silently disabling the alarm. */
ZTEST(ld81, test_short_circuit_is_a_fault_not_dry)
{
	reset_slot();

	zassert_equal(sample(SHORT_MV), 0);
	zassert_false(snap().valid, "a shorted sense line must not be reported as a dry sensor");

	zassert_equal(sample(0), 0);
	zassert_false(snap().valid, "0 mV must fault, not read as dry");
}

/* Losing a working sensor is itself an event: on a 30-minute report interval a
 * detector that goes away would otherwise be invisible for half an hour, and a
 * FLOODED slot going invalid looks exactly like silence. The data goes invalid on
 * the first faulty sample; the alarm comes once the fault is confirmed. */
ZTEST(ld81, test_losing_a_valid_reading_alarms)
{
	reset_slot();

	sample(DRY_MV);
	zassert_true(snap().valid);

	for (int i = 1; i < LD81_FAULT_CONFIRM_SAMPLES; i++) {
		zassert_equal(sample(OPEN_MV), 0, "an unconfirmed fault must not alarm yet");
		zassert_false(snap().valid, "a faulty sample must invalidate the slot at once");
	}

	zassert_equal(sample(OPEN_MV), APP_SENSOR_SAMPLE_ALARM,
		      "a working sensor going open-circuit must be reported once confirmed");
	zassert_equal(sample(OPEN_MV), 0, "still faulty must not re-alarm every sample");
}

/* A loose terminal that flips between a valid reading and an open circuit must not
 * raise an alarm on every flip, whether the sensor is dry or flooded. */
ZTEST(ld81, test_loose_terminal_does_not_flap)
{
	int alarms = 0;

	reset_slot();
	sample(DRY_MV);

	for (int i = 0; i < 10; i++) {
		alarms += sample(i % 2 ? DRY_MV : OPEN_MV) == APP_SENSOR_SAMPLE_ALARM;
	}

	zassert_equal(alarms, 0, "a flapping dry sensor alarmed %d times", alarms);

	zassert_equal(sample(FLOODED_MV), APP_SENSOR_SAMPLE_ALARM);
	alarms = 0;

	for (int i = 0; i < 10; i++) {
		alarms += sample(i % 2 ? FLOODED_MV : OPEN_MV) == APP_SENSOR_SAMPLE_ALARM;
	}

	zassert_equal(alarms, 0, "a flapping flooded sensor alarmed %d times", alarms);
}

/* A terminal that drops out for a few samples at a time raises one fault alarm per
 * LD81_FAULT_ALARM_HOLDOFF_S, not one per episode, whether the sensor is dry or flooded. */
ZTEST(ld81, test_burst_faults_alarm_once)
{
	const int levels[] = {DRY_MV, FLOODED_MV};

	for (int l = 0; l < ARRAY_SIZE(levels); l++) {
		int alarms = 0;

		reset_slot();
		sample(levels[l]);

		for (int i = 0; i < 30; i++) {
			alarms +=
				sample(i % 3 == 2 ? levels[l] : OPEN_MV) == APP_SENSOR_SAMPLE_ALARM;
		}

		zassert_equal(alarms, 1, "bursts of faults at %d mV alarmed %d times", levels[l],
			      alarms);
	}
}

/* The hold-off only spaces fault alarms out; a fault after it has passed alarms again. */
ZTEST(ld81, test_fault_alarm_holdoff_expires)
{
	reset_slot();
	sample(DRY_MV);
	for (int i = 1; i < LD81_FAULT_CONFIRM_SAMPLES; i++) {
		sample(OPEN_MV);
	}
	zassert_equal(sample(OPEN_MV), APP_SENSOR_SAMPLE_ALARM, "the first fault alarms");
	sample(DRY_MV);

	for (int i = 1; i < LD81_FAULT_CONFIRM_SAMPLES; i++) {
		sample(OPEN_MV);
	}
	zassert_equal(sample(OPEN_MV), 0, "a second fault within the hold-off");
	sample(DRY_MV);

	k_sleep(K_SECONDS(LD81_FAULT_ALARM_HOLDOFF_S));

	for (int i = 1; i < LD81_FAULT_CONFIRM_SAMPLES; i++) {
		sample(OPEN_MV);
	}
	zassert_equal(sample(OPEN_MV), APP_SENSOR_SAMPLE_ALARM,
		      "a fault after the hold-off must alarm again");
}

/* Both ends of the range `sensor-ld81-threshold` accepts still work: a flood can be
 * detected at the top, and a flooded slot can read dry again at the bottom. */
ZTEST(ld81, test_threshold_range_is_usable)
{
	reset_slot();
	g_app_config.ld81_threshold_mv = LD81_THRESHOLD_MAX_MV;
	zassert_equal(sample(LD81_OPEN_CIRCUIT_MV - 1), APP_SENSOR_SAMPLE_ALARM,
		      "the top threshold must still allow a flood");
	zassert_true(snap().valid && snap().flooded);

	reset_slot();
	g_app_config.ld81_threshold_mv = LD81_THRESHOLD_MIN_MV;
	zassert_equal(sample(LD81_THRESHOLD_MIN_MV + 1), APP_SENSOR_SAMPLE_ALARM);
	zassert_equal(sample(LD81_SHORT_CIRCUIT_MV + 1), APP_SENSOR_SAMPLE_ALARM,
		      "at the bottom threshold a flooded slot must still read dry");
	zassert_true(snap().valid && !snap().flooded);
}

/* The guards bracket the threshold, so a reading inside the window still decides
 * flood purely on the threshold. The guards must not eat legitimate readings —
 * this is what pins the fail-safe window to the real dry-to-flooded swing. */
ZTEST(ld81, test_window_does_not_swallow_valid_readings)
{
	reset_slot();

	sample(DRY_MV);
	struct app_data_ld81 d = snap();

	zassert_true(d.valid, "the calibrated dry level must stay valid");
	zassert_false(d.flooded);

	sample(FLOODED_MV);
	d = snap();
	zassert_true(d.valid,
		     "the calibrated flooded level must stay valid, not read as open circuit");
	zassert_true(d.flooded);
}
