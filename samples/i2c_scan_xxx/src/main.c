/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <stdlib.h>
#include <string.h>

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static int i2c_scan(void)
{
	uint8_t cnt = 0, first = 0x04, last = 0x77;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

	if (!dev) {
		LOG_ERR("I2C: Device driver i2c0 not found.");
		return -ENODEV;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	for (uint8_t i = first; i <= last; i++) {

		struct i2c_msg msgs[1];
		uint8_t dst;
		/* Send the address to read from */
		msgs[0].buf = &dst;
		msgs[0].len = 0U;
		msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

		bool ok = i2c_transfer(dev, &msgs[0], 1, i) == 0;
		if (ok) {
			cnt++;
			LOG_INF("Device found: 0x%02x", i);
		}
	}

	LOG_INF("%u devices found on i2c0", cnt);

	return cnt;
}

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		LOG_INF("Alive");

		/* For debug */
		/* ctr_led_blink(ctr_led_mainboard, CTR_LED_CHANNEL_R, K_MSEC(500)); */

		int ret = i2c_scan();
		if (ret != 2) {
			LOG_ERR("I2C scan failed");
			for (;;) {
				k_sleep(K_MSEC(10));
			}
		}

		/* Wait 10 ms */
		k_sleep(K_MSEC(10));
	}

	return 0;
}
