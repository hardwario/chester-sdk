/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_k1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_k1));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	static const enum ctr_k1_channel channels[] = {
		CTR_K1_CHANNEL_1_DIFFERENTIAL,
		CTR_K1_CHANNEL_2_DIFFERENTIAL,
		CTR_K1_CHANNEL_3_DIFFERENTIAL,
		CTR_K1_CHANNEL_4_DIFFERENTIAL,
	};

	static const struct ctr_k1_calibration calibrations[] = {
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
	};

#if 0
	for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
		ret = ctr_k1_set_power(dev, channels[i], true);
		if (ret) {
			LOG_ERR("Call `ctr_k1_set_power` failed: %d", ret);
			k_oops();
		}
	}
#endif

	for (;;) {
		LOG_INF("Alive");

#if 1
		for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
			ret = ctr_k1_set_power(dev, channels[i], true);
			if (ret) {
				LOG_ERR("Call `ctr_k1_set_power` failed: %d", ret);
				k_oops();
			}
		}

		k_sleep(K_MSEC(100));
#endif

		struct ctr_k1_result results[ARRAY_SIZE(channels)] = {0};
		ret = ctr_k1_measure(dev, channels, ARRAY_SIZE(channels), calibrations, results);
		if (ret) {
			LOG_ERR("Call `ctr_k1_measure` failed: %d", ret);
			k_oops();
		}

#if 1
		for (size_t i = 0; i < ARRAY_SIZE(channels); i++) {
			ret = ctr_k1_set_power(dev, channels[i], false);
			if (ret) {
				LOG_ERR("Call `ctr_k1_set_power` failed: %d", ret);
				k_oops();
			}
		}
#endif

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
