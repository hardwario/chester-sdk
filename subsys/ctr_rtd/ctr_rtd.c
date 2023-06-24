/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_rtd.h>
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
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_rtd, CONFIG_CTR_RTD_LOG_LEVEL);

#define R0_PT100     100.0
#define R_MIN_PT100  4.0
#define R_MAX_PT100  400.0
#define R_MIN_PT1000 40.0
#define R_MAX_PT1000 4000.0
#define R0_PT1000    1000.0
#define R_REF	     1800.0
#define ADC_GAIN     1
#define A_ITS_90     3.9083e-3
#define B_ITS_90     -5.775e-7
#define A_IPTS_68    3.90802e-3
#define B_IPTS_68    -5.80195e-7

static double convert_r_to_t(double r, double r0, double a, double b)
{
	return (-r0 * a + sqrt(r0 * r0 * a * a - 4 * r0 * b * (r0 - r))) / (2 * r0 * b);
}

int ctr_rtd_read(enum ctr_rtd_channel channel, enum ctr_rtd_type type, float *temperature)
{
	int ret;

	const struct device *dev = NULL;
	enum ctr_x3_channel chan;

	switch (channel) {
	case CTR_RTD_CHANNEL_A1:
		LOG_INF("A1");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_a));
		chan = CTR_X3_CHANNEL_1;
		break;
	case CTR_RTD_CHANNEL_A2:
		LOG_INF("A2");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_a));
		chan = CTR_X3_CHANNEL_2;
		break;
	case CTR_RTD_CHANNEL_B1:
		LOG_INF("B1");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_b));
		chan = CTR_X3_CHANNEL_1;
		break;
	case CTR_RTD_CHANNEL_B2:
		LOG_INF("B2");
		dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_b));
		chan = CTR_X3_CHANNEL_2;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	if (!dev || !device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	double r0;
	double r_min;
	double r_max;

	switch (type) {
	case CTR_RTD_TYPE_PT100:
		r0 = R0_PT100;
		r_min = R_MIN_PT100;
		r_max = R_MAX_PT100;
		break;
	case CTR_RTD_TYPE_PT1000:
		r0 = R0_PT1000;
		r_min = R_MIN_PT1000;
		r_max = R_MAX_PT1000;
		break;
	default:
		LOG_ERR("Unknown type: %d", type);
		return -EINVAL;
	}

	int32_t result;
	ret = ctr_x3_measure(dev, chan, &result);
	if (ret) {
		LOG_ERR("Call `ctr_x3_measure` failed: %d", ret);
		return ret;
	}

	double r_rtd = R_REF * ((double)result / (ADC_GAIN * pow(2, 23)));

	if (r_rtd < r_min || r_rtd > r_max) {
		LOG_WRN("Resistance out of range");
		return -ERANGE;
	}

	*temperature = convert_r_to_t(r_rtd, r0, A_ITS_90, B_ITS_90);

	LOG_INF("Raw: %" PRId32 "; R: %.3f Ohm; T: %.3f C", result, r_rtd, *temperature);

	return 0;
}
