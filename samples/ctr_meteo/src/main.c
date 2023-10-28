/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_meteo.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_meteo_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		float rainfall;
		ret = ctr_meteo_get_rainfall_and_clear(dev, &rainfall);
		if (ret) {
			LOG_ERR("Call `ctr_meteo_get_rainfall_and_clear` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Rainfall: %.2f mm", rainfall);

		float speed;
		ret = ctr_meteo_get_wind_speed_and_clear(dev, &speed);
		if (ret) {
			LOG_ERR("Call `ctr_meteo_get_wind_speed_and_clear` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Wind speed: %.2f m/s", speed);

		float direction;
		ret = ctr_meteo_get_wind_direction(dev, &direction);
		if (ret) {
			LOG_ERR("Call `ctr_meteo_get_wind_direction` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Wind direction: %.2f deg", direction);

		k_sleep(K_MSEC(5000));
	}

	return 0;
}
