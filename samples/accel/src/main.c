/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

		float accel_x;
		float accel_y;
		float accel_z;
		int orientation;
		ret = ctr_accel_read(&accel_x, &accel_y, &accel_z, &orientation);
		if (ret) {
			LOG_ERR("Call `ctr_accel_read` failed: %d", ret);

		} else {
			LOG_INF("Acceleration X: %.3f m/s^2", accel_x);
			LOG_INF("Acceleration Y: %.3f m/s^2", accel_y);
			LOG_INF("Acceleration Z: %.3f m/s^2", accel_z);
			LOG_INF("Orientation: %d", orientation);
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
