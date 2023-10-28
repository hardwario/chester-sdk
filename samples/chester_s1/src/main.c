/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#define TGT_CO2_CONC 420

void ctr_s1_event_handler(const struct device *dev, enum ctr_s1_event event, void *user_data)
{
	int ret;

	switch (event) {
	case CTR_S1_EVENT_DEVICE_RESET:
		LOG_INF("Event `CTR_S1_EVENT_DEVICE_RESET`");

		ret = ctr_s1_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
			k_oops();
		}

		break;

	case CTR_S1_EVENT_MOTION_DETECTED:
		LOG_INF("Event `CTR_S1_EVENT_MOTION_DETECTED`");

		int motion_count;
		ret = ctr_s1_read_motion_count(dev, &motion_count);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_motion_count` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Motion count: %d", motion_count);
		break;

	case CTR_S1_EVENT_BUTTON_PRESSED:
		LOG_INF("Event `CTR_S1_EVENT_BUTTON_PRESSED`");

		struct ctr_s1_led_param param_led = {
			.brightness = CTR_S1_LED_BRIGHTNESS_HIGH,
			.command = CTR_S1_LED_COMMAND_1X_1_1,
			.pattern = CTR_S1_LED_PATTERN_NONE,
		};

		ret = ctr_s1_set_led(dev, CTR_S1_LED_CHANNEL_B, &param_led);
		if (ret) {
			LOG_ERR("Call `ctr_s1_set_led` failed: %d", ret);
			k_oops();
		}

		struct ctr_s1_buzzer_param param_buzzer = {
			.command = CTR_S1_BUZZER_COMMAND_1X_1_1,
			.pattern = CTR_S1_BUZZER_PATTERN_NONE,
		};

		ret = ctr_s1_set_buzzer(dev, &param_buzzer);
		if (ret) {
			LOG_ERR("Call `ctr_s1_set_buzzer` failed: %d", ret);
			k_oops();
		}

		ret = ctr_s1_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
			k_oops();
		}

		break;

	case CTR_S1_EVENT_BUTTON_HOLD:
		LOG_INF("Event `CTR_S1_EVENT_BUTTON_HOLD`");

		ret = ctr_s1_calib_tgt_co2_conc(dev, TGT_CO2_CONC);
		if (ret) {
			LOG_ERR("Call `ctr_s1_calib_tgt_co2_conc` failed: %d", ret);
			k_oops();
		}

		break;

	default:
		LOG_DBG("Unhandled event: %d", event);
		break;
	}
}

int main(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_s1_set_handler(dev, ctr_s1_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_s1_set_handler` failed: %d", ret);
		k_oops();
	}

	ret = ctr_s1_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_s1_enable_interrupts` failed: %d", ret);
		k_oops();
	}

	uint32_t serial_number;
	ret = ctr_s1_get_serial_number(dev, &serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_s1_get_serial_number` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Serial number: %u", serial_number);

	uint16_t hw_revision;
	ret = ctr_s1_get_hw_revision(dev, &hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_s1_get_hw_revision` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Hardware revision: 0x%04x", hw_revision);

	uint32_t hw_variant;
	ret = ctr_s1_get_hw_variant(dev, &hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_s1_get_hw_variant` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Hardware variant: 0x%08x", hw_variant);

	uint32_t fw_version;
	ret = ctr_s1_get_fw_version(dev, &fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_s1_get_fw_version` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Firmware version: 0x%08x", fw_version);

	for (;;) {
		LOG_INF("Alive");

		float temperature;
		ret = ctr_s1_read_temperature(dev, &temperature);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_temperature` failed: %d", ret);
		} else {
			LOG_INF("IAQ: Temperature: %.1f C", temperature);
		}

		float humidity;
		ret = ctr_s1_read_humidity(dev, &humidity);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_humidity` failed: %d", ret);
		} else {
			LOG_INF("IAQ: Humidity: %.1f %%", humidity);
		}

		float co2_conc;
		ret = ctr_s1_read_co2_conc(dev, &co2_conc);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_co2_conc` failed: %d", ret);
		} else {
			LOG_INF("IAQ: CO2 conc.: %.0f ppm", co2_conc);
		}

		float illuminance;
		ret = ctr_s1_read_illuminance(dev, &illuminance);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_illuminance` failed: %d", ret);
		} else {
			LOG_INF("IAQ: Illuminance: %.0f lux", illuminance);
		}

		float pressure;
		ret = ctr_s1_read_pressure(dev, &pressure);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_pressure` failed: %d", ret);
		} else {
			LOG_INF("IAQ: Pressure: %.0f Pa", pressure);
		}

		float altitude;
		ret = ctr_s1_read_altitude(dev, &altitude);
		if (ret) {
			LOG_ERR("Call `ctr_s1_read_altitude` failed: %d", ret);
		} else {
			LOG_INF("IAQ: Altitude: %.0f m", altitude);
		}

		k_sleep(K_SECONDS(10));
	}

	return 0;
}
