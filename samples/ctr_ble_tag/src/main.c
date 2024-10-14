/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_buf.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	char _msg_buf[512];
	char intermediate_buf[64];
	struct ctr_buf msg_buf;
	ctr_buf_init(&msg_buf, (void *)_msg_buf, 256);

	for (;;) {
		k_sleep(K_SECONDS(30));

		LOG_DBG("Alive");

		for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
			uint8_t addr[BT_ADDR_SIZE];
			int rssi;
			float voltage;
			float temperature;
			float humidity;
			bool magnet_detected;
			bool moving;
			int movement_event_count;
			float roll;
			float pitch;
			bool low_battery;

			int16_t sensor_mask;
			bool valid;

			ret = ctr_ble_tag_read_cached(slot, addr, &rssi, &voltage, &temperature,
						      &humidity, &magnet_detected, &moving,
						      &movement_event_count, &roll, &pitch,
						      &low_battery, &sensor_mask, &valid);
			if (ret && !valid) {
				continue;
			}

			char addr_str[BT_ADDR_SIZE * 2 + 1];

			ret = ctr_buf2hex(addr, BT_ADDR_SIZE, addr_str, sizeof(addr_str), false);
			if (ret < 0) {
				LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
				return ret;
			}

			ctr_buf_fill(&msg_buf, 0);

			snprintf(intermediate_buf, 32, "addr %u: %s ", slot, addr_str);
			ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
			if (ret) {
				LOG_ERR("Call `ctr_buf_append_str` failed");
				return ret;
			}
			msg_buf.len--;

			snprintf(intermediate_buf, 32, "rssi: %d dBm ", rssi);
			ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
			if (ret) {
				LOG_ERR("Call `ctr_buf_append_str` failed");
				return ret;
			}
			msg_buf.len--;

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_VOLTAGE) {
				snprintf(intermediate_buf, 32, "/ voltage: %.2f V ", voltage);
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_TEMPERATURE) {
				snprintf(intermediate_buf, 32, "/ temperature: %.2f C ",
					 temperature);
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_HUMIDITY) {
				snprintf(intermediate_buf, 32, "/ humidity: %.2f %% ", humidity);
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MAGNET_DETECTED) {
				snprintf(intermediate_buf, 64, "/ magnetic sensor: %s ",
					 magnet_detected ? "detected" : "not detected");
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVING) {
				snprintf(intermediate_buf, 32, "/ moving: %s ",
					 moving ? "true" : "false");
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_MOVEMENT_EVENT_COUNT) {
				snprintf(intermediate_buf, 64, "/ movement event count: %d ",
					 movement_event_count);
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_ROLL) {
				snprintf(intermediate_buf, 32, "/ roll: %.2f ", roll);
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_PITCH) {
				snprintf(intermediate_buf, 32, "/ pitch: %.2f ", pitch);
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			if (sensor_mask & CTR_BLE_TAG_SENSOR_MASK_LOW_BATTERY) {
				snprintf(intermediate_buf, 32, "/ low battery: %s",
					 low_battery ? "true" : "false");
				ret = ctr_buf_append_str(&msg_buf, intermediate_buf);
				if (ret) {
					LOG_ERR("Call `ctr_buf_append_str` failed");
					return ret;
				}
				msg_buf.len--;
			}

			LOG_DBG("%s", ctr_buf_get_mem(&msg_buf));
		}
	}

	return 0;
}
