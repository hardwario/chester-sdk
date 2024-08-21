/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_measure.h"

/* CHESTER includes */
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_x3.h>
#include <chester/drivers/people_counter.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>

/* Standard includes */
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(app_measure, LOG_LEVEL_DBG);

#define MAX_REPETITIONS 5
#define MAX_DIFFERENCE  100

#if defined(FEATURE_HARDWARE_CHESTER_X3_A) || defined(FEATURE_HARDWARE_CHESTER_X3_B)

static int compare(const void *a, const void *b)
{
	int32_t sample_a = *(int32_t *)a;
	int32_t sample_b = *(int32_t *)b;

	if (sample_a == sample_b) {
		return 0;
	} else if (sample_a < sample_b) {
		return -1;
	}

	return 1;
}

enum measure_weight_slot {
	MEASURE_WEIGHT_SLOT_A,
	MEASURE_WEIGHT_SLOT_B,
};

static int read_weight(const char *id, enum measure_weight_slot slot, enum ctr_x3_channel channel,
		       int32_t *result)
{
	int ret;

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	static const struct device *ctr_x3_a_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_a));
#endif /* defined(FEATURE_HARDWARE_CHESTER_X3_A) */

#if defined(FEATURE_HARDWARE_CHESTER_X3_B)
	static const struct device *ctr_x3_b_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_b));
#endif /* defined(FEATURE_HARDWARE_CHESTER_X3_B) */

	const struct device *dev;

	switch (slot) {
#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	case MEASURE_WEIGHT_SLOT_A:
		dev = ctr_x3_a_dev;
		break;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X3_A) */
#if defined(FEATURE_HARDWARE_CHESTER_X3_B)
	case MEASURE_WEIGHT_SLOT_B:
		dev = ctr_x3_b_dev;
		break;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X3_B) */
	default:
		LOG_ERR("Unknown slot: %d", slot);
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_x3_set_power(dev, channel, true);
	if (ret) {
		LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
		goto error;
	}

	k_sleep(K_MSEC(100));

	int32_t samples[32];
	int64_t average = 0;
	int64_t filtered = 0;

	for (size_t i = 0; i < ARRAY_SIZE(samples); i++) {
		int32_t sample;
		ret = ctr_x3_measure(dev, channel, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_x3_measure` failed: %d", ret);
			goto error;
		}

		samples[i] = sample;
		average += sample;
	}

	average /= ARRAY_SIZE(samples);

	qsort(samples, ARRAY_SIZE(samples), sizeof(samples[0]), compare);

	size_t start = ARRAY_SIZE(samples) / 4;
	size_t stop = 3 * ARRAY_SIZE(samples) / 4;

	for (size_t i = start; i < stop; i++) {
		filtered += samples[i];
	}

	filtered /= stop - start;

	ret = ctr_x3_set_power(dev, channel, false);
	if (ret) {
		LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
		goto error;
	}

	*result = filtered;

	LOG_DBG("Channel %s: Minimum: %" PRId32, id, samples[0]);
	LOG_DBG("Channel %s: Maximum: %" PRId32, id, samples[ARRAY_SIZE(samples) - 1]);
	LOG_DBG("Channel %s: Average: %" PRId32, id, (int32_t)average);
	LOG_DBG("Channel %s: Median: %" PRId32, id, samples[ARRAY_SIZE(samples) / 2]);
	LOG_DBG("Channel %s: Filtered: %" PRId32, id, (int32_t)filtered);

	return 0;

error:
	ctr_x3_set_power(dev, CTR_X3_CHANNEL_1, false);
	return ret;
}

static int filter_weight(const char *id, enum measure_weight_slot slot, enum ctr_x3_channel channel,
			 int32_t *result, int32_t *prev)
{
	int ret;

	int i;
	for (i = 0; i < MAX_REPETITIONS; i++) {
		if (i != 0) {
			LOG_INF("Channel %s: Repeating measurement (%d)", id, i);
		}

		ret = read_weight(id, slot, channel, result);
		if (ret) {
			LOG_ERR("Call `read_weight` failed: %d", ret);
			return ret;
		}

		if (*prev == INT32_MAX) {
			break;
		}

		int32_t diff = MAX(*result, *prev) - MIN(*result, *prev);

		if (diff < MAX_DIFFERENCE) {
			break;
		} else {
			*prev = *result;
		}
	}

	*prev = *result;

	if (i == MAX_REPETITIONS) {
		*result ^= BIT(30);
		LOG_WRN("Channel %s: Inverted ERROR flag (bit 30)", id);
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X3_A) || defined(FEATURE_HARDWARE_CHESTER_X3_B) */

int app_measure_weight(void)
{
	int ret;

	if (g_app_data.weight_measurement_count >= ARRAY_SIZE(g_app_data.weight_measurements)) {
		LOG_WRN("Weight measurements buffer full");
		return -ENOSPC;
	}

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	app_data_lock();

	int32_t a1_raw = INT32_MAX;
	int32_t a2_raw = INT32_MAX;
	int32_t b1_raw = INT32_MAX;
	int32_t b2_raw = INT32_MAX;

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	if (g_app_config.channel_a1_active) {
		static int32_t a1_raw_prev = INT32_MAX;
		ret = filter_weight("A1", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_1, &a1_raw,
				    &a1_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (A1): %d", ret);
			a1_raw = INT32_MAX;
		}
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	if (g_app_config.channel_a2_active) {
		static int32_t a2_raw_prev = INT32_MAX;
		ret = filter_weight("A2", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_2, &a2_raw,
				    &a2_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (A2): %d", ret);
			a2_raw = INT32_MAX;
		}
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X3_B)
	if (g_app_config.channel_b1_active) {
		static int32_t b1_raw_prev = INT32_MAX;
		ret = filter_weight("B1", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_1, &b1_raw,
				    &b1_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (B1): %d", ret);
			b1_raw = INT32_MAX;
		}
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X3_B)
	if (g_app_config.channel_b2_active) {
		static int32_t b2_raw_prev = INT32_MAX;
		ret = filter_weight("B2", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_2, &b2_raw,
				    &b2_raw_prev);
		if (ret) {
			LOG_ERR("Call `filter_weight` failed (B2): %d", ret);
			b2_raw = INT32_MAX;
		}
	}
#endif

	g_app_data.weight_measurements[g_app_data.weight_measurement_count].timestamp_offset =
		timestamp - g_app_data.weight_measurement_timestamp;

	g_app_data.weight_measurements[g_app_data.weight_measurement_count].a1_raw = a1_raw;
	g_app_data.weight_measurements[g_app_data.weight_measurement_count].a2_raw = a2_raw;
	g_app_data.weight_measurements[g_app_data.weight_measurement_count].b1_raw = b1_raw;
	g_app_data.weight_measurements[g_app_data.weight_measurement_count].b2_raw = b2_raw;

	g_app_data.weight_measurement_count++;

	app_data_unlock();
	return 0;
}

#if defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER)

int app_measure_people(void)
{
	int ret;

	if (g_app_data.people_measurement_count >= ARRAY_SIZE(g_app_data.people_measurements)) {
		LOG_WRN("People measurements buffer full");
		return -ENOSPC;
	}

	int64_t timestamp;
	ret = ctr_rtc_get_ts(&timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(people_counter));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	app_data_lock();
	g_app_data.people_measurements[g_app_data.people_measurement_count].timestamp_offset =
		timestamp - g_app_data.people_measurement_timestamp;

	struct people_counter_measurement measurement;
	ret = people_counter_read_measurement(dev, &measurement);
	if (ret) {
		LOG_ERR("Call `people_counter_read_measurement` failed: %d", ret);

		g_app_data.people_measurements[g_app_data.people_measurement_count].is_valid =
			false;

	} else {
		LOG_INF("Motion counter: %u", measurement.motion_counter);
		LOG_INF("Pass counter (left): %u", measurement.pass_counter_left);
		LOG_INF("Pass counter (right): %u", measurement.pass_counter_right);
		LOG_INF("Pass counter (adult): %u", measurement.pass_counter_adult);
		LOG_INF("Pass counter (child): %u", measurement.pass_counter_child);
		LOG_INF("Stay counter (adult): %u", measurement.stay_counter_adult);
		LOG_INF("Stay counter (child): %u", measurement.stay_counter_child);
		LOG_INF("Total time (adult): %u", measurement.total_time_adult);
		LOG_INF("Total time (child): %u", measurement.total_time_child);
		LOG_INF("Consumed energy: %u", measurement.consumed_energy);

		g_app_data.people_measurements[g_app_data.people_measurement_count].measurement =
			measurement;

		g_app_data.people_measurements[g_app_data.people_measurement_count].is_valid = true;
	}

	g_app_data.people_measurement_count++;
	app_data_unlock();
	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER) */

int app_test_measure(const struct shell *sh)
{
#if defined(FEATURE_HARDWARE_CHESTER_X3_A) || defined(FEATURE_HARDWARE_CHESTER_X3_B)
	int ret;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X3_A) || defined(FEATURE_HARDWARE_CHESTER_X3_B) */

	app_data_lock();

	int32_t a1_raw = INT32_MAX;
	int32_t a2_raw = INT32_MAX;
	int32_t b1_raw = INT32_MAX;
	int32_t b2_raw = INT32_MAX;

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	static int32_t a1_raw_prev = INT32_MAX;
	ret = filter_weight("A1", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_1, &a1_raw, &a1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A1): %d", ret);
		shell_error(sh, "channel measurement failed (a1)");
		a1_raw = INT32_MAX;
	}
#endif

	if (a1_raw == INT32_MAX) {
		shell_print(sh, "raw (a1): (null)");
	} else {
		shell_print(sh, "raw (a1): %" PRId32, a1_raw);
	}

#if defined(FEATURE_HARDWARE_CHESTER_X3_A)
	static int32_t a2_raw_prev = INT32_MAX;
	ret = filter_weight("A2", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_2, &a2_raw, &a2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A2): %d", ret);
		shell_error(sh, "channel measurement failed (a2)");
		a2_raw = INT32_MAX;
	}
#endif

	if (a2_raw == INT32_MAX) {
		shell_print(sh, "raw (a2): (null)");
	} else {
		shell_print(sh, "raw (a2): %" PRId32, a2_raw);
	}

#if defined(FEATURE_HARDWARE_CHESTER_X3_B)
	static int32_t b1_raw_prev = INT32_MAX;
	ret = filter_weight("B1", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_1, &b1_raw, &b1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B1): %d", ret);
		shell_error(sh, "channel measurement failed (b1)");
		b1_raw = INT32_MAX;
	}
#endif

	if (b1_raw == INT32_MAX) {
		shell_print(sh, "raw (b1): (null)");
	} else {
		shell_print(sh, "raw (b1): %" PRId32, b1_raw);
	}

#if defined(FEATURE_HARDWARE_CHESTER_X3_B)
	static int32_t b2_raw_prev = INT32_MAX;
	ret = filter_weight("B2", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_2, &b2_raw, &b2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B2): %d", ret);
		shell_error(sh, "channel measurement failed (b2)");
		b2_raw = INT32_MAX;
	}
#endif

	if (b2_raw == INT32_MAX) {
		shell_print(sh, "raw (b2): (null)");
	} else {
		shell_print(sh, "raw (b2): %" PRId32, b2_raw);
	}

	app_data_unlock();

	return 0;
}

int app_measure_clear(void)
{
	int ret;

	app_data_lock();
	g_app_data.weight_measurement_count = 0;

	ret = ctr_rtc_get_ts(&g_app_data.weight_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Set base timestamp for weight measurement: %llu",
		g_app_data.weight_measurement_timestamp);

#if defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER)
	g_app_data.people_measurement_count = 0;

	ret = ctr_rtc_get_ts(&g_app_data.people_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Set base timestamp for people measurement: %llu",
		g_app_data.people_measurement_timestamp);
#endif /* defined(FEATURE_HARDWARE_CHESTER_PEOPLE_COUNTER) */
	app_data_unlock();

	return 0;
}