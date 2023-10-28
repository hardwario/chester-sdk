/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_therm.h>
#include <chester/drivers/ctr_x3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define MAX_REPETITIONS     5
#define MAX_DIFFERENCE      100
#define STABILIZATION_DELAY K_MSEC(300)
#define MEASUREMENT_PAUSE   K_SECONDS(2)

#if defined(CONFIG_SHIELD_CTR_X3_A) || defined(CONFIG_SHIELD_CTR_X3_B)

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

#if defined(CONFIG_SHIELD_CTR_X3_A)
	static const struct device *ctr_x3_a_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_a));
#endif /* defined(CONFIG_SHIELD_CTR_X3_A) */

#if defined(CONFIG_SHIELD_CTR_X3_B)
	static const struct device *ctr_x3_b_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x3_b));
#endif /* defined(CONFIG_SHIELD_CTR_X3_B) */

	const struct device *dev;

	switch (slot) {
#if defined(CONFIG_SHIELD_CTR_X3_A)
	case MEASURE_WEIGHT_SLOT_A:
		dev = ctr_x3_a_dev;
		break;
#endif /* defined(CONFIG_SHIELD_CTR_X3_A) */
#if defined(CONFIG_SHIELD_CTR_X3_B)
	case MEASURE_WEIGHT_SLOT_B:
		dev = ctr_x3_b_dev;
		break;
#endif /* defined(CONFIG_SHIELD_CTR_X3_B) */
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

	k_sleep(STABILIZATION_DELAY);

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

	ret = ctr_x3_set_power(dev, channel, false);
	if (ret) {
		LOG_ERR("Call `ctr_x3_set_power` failed: %d", ret);
		goto error;
	}

	for (size_t i = 0; i < ARRAY_SIZE(samples); i++) {
		LOG_INF("Channel %s: Sample %u: %" PRId32, id, i, samples[i]);
	}

	average /= ARRAY_SIZE(samples);

	qsort(samples, ARRAY_SIZE(samples), sizeof(samples[0]), compare);

	size_t start = ARRAY_SIZE(samples) / 4;
	size_t stop = 3 * ARRAY_SIZE(samples) / 4;

	for (size_t i = start; i < stop; i++) {
		filtered += samples[i];
	}

	filtered /= stop - start;

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
		LOG_WRN("Channel %s: Too many repetitions", id);
	}

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_X3_A) || defined(CONFIG_SHIELD_CTR_X3_B) */

static int measure(void)
{
	int ret;

	int32_t a1_raw = INT32_MAX;
	int32_t a2_raw = INT32_MAX;
	int32_t b1_raw = INT32_MAX;
	int32_t b2_raw = INT32_MAX;

#if defined(CONFIG_SHIELD_CTR_X3_A)
	static int32_t a1_raw_prev = INT32_MAX;
	ret = filter_weight("A1", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_1, &a1_raw, &a1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A1): %d", ret);
		a1_raw = INT32_MAX;
	}
#endif /* defined(CONFIG_SHIELD_CTR_X3_A) */

#if defined(CONFIG_SHIELD_CTR_X3_A)
	static int32_t a2_raw_prev = INT32_MAX;
	ret = filter_weight("A2", MEASURE_WEIGHT_SLOT_A, CTR_X3_CHANNEL_2, &a2_raw, &a2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (A2): %d", ret);
		a2_raw = INT32_MAX;
	}
#endif /* defined(CONFIG_SHIELD_CTR_X3_A) */

#if defined(CONFIG_SHIELD_CTR_X3_B)
	static int32_t b1_raw_prev = INT32_MAX;
	ret = filter_weight("B1", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_1, &b1_raw, &b1_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B1): %d", ret);
		b1_raw = INT32_MAX;
	}
#endif /* defined(CONFIG_SHIELD_CTR_X3_B) */

#if defined(CONFIG_SHIELD_CTR_X3_B)
	static int32_t b2_raw_prev = INT32_MAX;
	ret = filter_weight("B2", MEASURE_WEIGHT_SLOT_B, CTR_X3_CHANNEL_2, &b2_raw, &b2_raw_prev);
	if (ret) {
		LOG_ERR("Call `filter_weight` failed (B2): %d", ret);
		b2_raw = INT32_MAX;
	}
#endif /* defined(CONFIG_SHIELD_CTR_X3_B) */

	if (a1_raw != INT32_MAX) {
		LOG_INF("A1 raw: %d", a1_raw);
	} else {
		LOG_INF("A1 raw: N/A");
	}

	if (a2_raw != INT32_MAX) {
		LOG_INF("A2 raw: %d", a2_raw);
	} else {
		LOG_INF("A2 raw: N/A");
	}

	if (b1_raw != INT32_MAX) {
		LOG_INF("B1 raw: %d", b1_raw);
	} else {
		LOG_INF("B1 raw: N/A");
	}

	if (b2_raw != INT32_MAX) {
		LOG_INF("B2 raw: %d", b2_raw);
	} else {
		LOG_INF("B2 raw: N/A");
	}

	return 0;
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	int cycles = 0;

	for (;;) {
		LOG_INF("Alive");

		LOG_INF("Cycle start: %d", ++cycles);

		float temperature;
		ret = ctr_therm_read(&temperature);
		if (ret) {
			LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
			goto error;
		}

		LOG_INF("Temperature: %.2f C", temperature);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(100));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		ret = measure();
		if (ret) {
			LOG_ERR("Call `measure` failed: %d", ret);
			goto error;
		}

		LOG_INF("Cycle stop");

	error:
		k_sleep(MEASUREMENT_PAUSE);
	}

	return 0;
}
