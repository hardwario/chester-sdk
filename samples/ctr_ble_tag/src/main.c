/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		k_sleep(K_SECONDS(30));

		LOG_DBG("Alive");

		for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
			uint8_t addr[BT_ADDR_SIZE];
			int rssi;
			float voltage;
			float temperature;
			float humidity;
			bool valid;

			ret = ctr_ble_tag_read(i, addr, &rssi, &voltage, &temperature, &humidity,
					       &valid);
			if (ret && ret != -ENOENT) {
				LOG_ERR("Call `ctr_ble_tag_read` failed: %d", ret);
				continue;
			}

			char addr_str[BT_ADDR_SIZE * 2 + 1];
			if (ctr_buf2hex(addr, sizeof(addr), addr_str, sizeof(addr_str), true) < 0) {
				LOG_ERR("Call `ctr_buf2hex` failed: %d", -EINVAL);
			}

			if (ret == -ENOENT) {
				LOG_INF("Tag %d , Address: %s: No data", i, addr_str);
			} else {
				LOG_INF("Tag %d , Address %s: Voltage: %.2f V / Temperature: %.2f "
					"ºC / Humidity: %.0f %% / RSSI: %d dbm",
					i, addr_str, voltage, temperature, humidity, rssi);
			}
		}
	}

	return 0;
}
