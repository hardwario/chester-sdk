/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

void ctr_z_event_handler(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	int ret;

	LOG_DBG("Event: %d", event);

	switch (event) {
	case CTR_Z_EVENT_DEVICE_RESET:
		ret = ctr_z_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_z_apply` failed: %d", ret);
			k_oops();
		}
		return;
	case CTR_Z_EVENT_BUTTON_0_PRESS:
	case CTR_Z_EVENT_BUTTON_1_PRESS:
	case CTR_Z_EVENT_BUTTON_2_PRESS:
	case CTR_Z_EVENT_BUTTON_3_PRESS:
	case CTR_Z_EVENT_BUTTON_4_PRESS:
		break;
	default:
		return;
	}

	static int led;

	struct ctr_z_led_param param = {
		.brightness = CTR_Z_LED_BRIGHTNESS_HIGH,
		.command = CTR_Z_LED_COMMAND_1X_1_2,
		.pattern = CTR_Z_LED_PATTERN_OFF,
	};

	ret = ctr_z_set_led(dev, led++ % 15, &param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		k_oops();
	}

	ret = ctr_z_apply(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_apply` failed: %d", ret);
		k_oops();
	}
}

int main(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_z_set_handler(dev, ctr_z_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
		k_oops();
	}

	ret = ctr_z_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_enable_interrupts` failed: %d", ret);
		k_oops();
	}

	uint32_t serial_number;
	ret = ctr_z_get_serial_number(dev, &serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_serial_number` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Serial number: %u", serial_number);

	uint16_t hw_revision;
	ret = ctr_z_get_hw_revision(dev, &hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_revision` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Hardware revision: 0x%04x", hw_revision);

	uint32_t hw_variant;
	ret = ctr_z_get_hw_variant(dev, &hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_variant` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Hardware variant: 0x%08x", hw_variant);

	uint32_t fw_version;
	ret = ctr_z_get_fw_version(dev, &fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Firmware version: 0x%08x", fw_version);

	for (;;) {
		uint16_t vdc;
		ret = ctr_z_get_vdc_mv(dev, &vdc);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Voltage VDC: %u mV", vdc);

		uint16_t vbat;
		ret = ctr_z_get_vbat_mv(dev, &vbat);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Voltage VBAT: %u mV", vbat);

		k_sleep(K_MSEC(1000));
	}

	return 0;
}
