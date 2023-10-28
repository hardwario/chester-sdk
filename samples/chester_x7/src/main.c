/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x7.h>

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

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x7_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	static const struct ctr_x7_calibration calibrations[] = {
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
		{.x0 = NAN, .y0 = NAN, .x1 = NAN, .y1 = NAN},
	};

#if 0
	ret = ctr_x7_set_power(dev, true);
	if (ret) {
		LOG_ERR("Call `ctr_x7_set_power` failed: %d", ret);
		k_oops();
	}

	k_sleep(K_MSEC(100));
#endif

	for (;;) {
		LOG_INF("Alive");

#if 1
		ret = ctr_x7_set_power(dev, true);
		if (ret) {
			LOG_ERR("Call `ctr_x7_set_power` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(100));
#endif

		struct ctr_x7_result results[2] = {0};
		ret = ctr_x7_measure(dev, calibrations, results);
		if (ret) {
			LOG_ERR("Call `ctr_x7_measure` failed: %d", ret);
			k_oops();
		}

#if 1
		ret = ctr_x7_set_power(dev, false);
		if (ret) {
			LOG_ERR("Call `ctr_x7_set_power` failed: %d", ret);
			k_oops();
		}
#endif

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
