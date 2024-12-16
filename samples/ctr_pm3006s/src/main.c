/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_pm3006s.h>
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_pm3006s));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_pm3006s_open_measurement(dev);
		if (ret) {
			LOG_ERR("Call `ctr_pm3006s_open_measurement` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(3));

		struct ctr_pm3006s_measurement measurement;
		ret = ctr_pm3006s_read_measurement(dev, &measurement);
		if (ret) {
			LOG_ERR("Call `ctr_pm3006s_read_measurement` failed: %d", ret);
			k_oops();
		} else {
			LOG_INF("Sensor status: %u", measurement.sensor_status);
			LOG_INF("Total Suspended Particles (TSP): %u ug/m", measurement.tsp);
			LOG_INF("PM1: %u ug/m", measurement.pm1);
			LOG_INF("PM2.5: %u ug/m", measurement.pm2_5);
			LOG_INF("PM10: %u ug/m", measurement.pm10);
			LOG_INF("0.3 um particles: %u pcs/l", measurement.q0_3um);
			LOG_INF("0.5 um particles: %u pcs/l", measurement.q0_5um);
			LOG_INF("1.0 um particles: %u pcs/l", measurement.q1_0um);
			LOG_INF("2.5 um particles: %u pcs/l", measurement.q2_5um);
			LOG_INF("5.0 um particles: %u pcs/l", measurement.q5_0um);
			LOG_INF("10.0 um particles: %u pcs/l", measurement.q10um);
		}

		k_sleep(K_SECONDS(8));

		ret = ctr_pm3006s_close_measurement(dev);
		if (ret) {
			LOG_ERR("Call `ctr_pm3006s_close_measurement` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(10));
	}

	return 0;
}
