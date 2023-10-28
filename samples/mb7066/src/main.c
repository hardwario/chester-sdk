/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Sensor wiring for X0 in slot A:
 * | A2 | CH1 | Sensor power  |
 * | A3 | GND | Sensor ground |
 * | A4 | CH2 | Sensor signal |
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/drivers/mb7066.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *mb7066 = DEVICE_DT_GET(DT_NODELABEL(mb7066));

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (!device_is_ready(mb7066)) {
		LOG_ERR("mb7066 is not ready");
		k_oops();
	}

	for (;;) {
		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(30));
		ctr_led_set(CTR_LED_CHANNEL_G, false);

		float val;
		ret = mb7066_measure(mb7066, &val);

		if (!ret) {
			LOG_INF("Measured: %d mm", (int)(val * 1000));
		} else {
			LOG_WRN("Sensor error: %d", ret);
		}

		k_sleep(K_MSEC(3000));
	}

	return 0;
}
