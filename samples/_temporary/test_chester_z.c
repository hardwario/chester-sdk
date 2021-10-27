#include "test_chester_z.h"
#include <ctr_chester_z.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(test_chester_z, LOG_LEVEL_DBG);

void chester_z_event_handler(enum hio_chester_z_event event, void *param)
{
	int ret;

	switch (event) {
	case HIO_CHESTER_Z_EVENT_BUTTON_1_CLICK: {
		struct hio_chester_z_buzzer_param param = {
			.command = HIO_CHESTER_Z_BUZZER_COMMAND_1X_1_1,
			.pattern = HIO_CHESTER_Z_BUZZER_PATTERN_NONE
		};

		ret = hio_chester_z_set_buzzer(&param);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_set_buzzer` failed: %d", ret);
			k_oops();
		}

		ret = hio_chester_z_apply();

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_apply` failed: %d", ret);
			k_oops();
		}

		break;
	}

	case HIO_CHESTER_Z_EVENT_BUTTON_2_CLICK: {
		struct hio_chester_z_led_param param = {
			.brightness = HIO_CHESTER_Z_LED_BRIGHTNESS_HIGH,
			.command = HIO_CHESTER_Z_LED_COMMAND_4X_1_16,
			.pattern = HIO_CHESTER_Z_LED_PATTERN_NONE
		};

		ret = hio_chester_z_set_led(HIO_CHESTER_Z_LED_CHANNEL_1_R, &param);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_set_buzzer` failed: %d", ret);
			k_oops();
		}

		ret = hio_chester_z_apply();

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_apply` failed: %d", ret);
			k_oops();
		}

		break;
	}

	default:
		break;
	}
}

int test_chester_z(void)
{
	int ret;

	LOG_INF("Start");

	ret = hio_chester_z_init(chester_z_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_init` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;

	ret = hio_chester_z_get_serial_number(&serial_number);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_get_serial_number` failed: %d", ret);
		return ret;
	}

	LOG_INF("Serial number: %08x", serial_number);

	uint16_t hw_revision;

	ret = hio_chester_z_get_hw_revision(&hw_revision);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_get_hw_revision` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW revision: %04x", hw_revision);

	uint32_t hw_variant;

	ret = hio_chester_z_get_hw_variant(&hw_variant);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_get_hw_variant` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW variant: %08x", hw_variant);

	uint32_t fw_version;

	ret = hio_chester_z_get_fw_version(&fw_version);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_get_fw_version` failed: %d", ret);
		return ret;
	}

	LOG_INF("FW version: %08x", fw_version);

	char *vendor_name;

	ret = hio_chester_z_get_vendor_name(&vendor_name);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_get_vendor_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Vendor name: %s", vendor_name);

	char *product_name;

	ret = hio_chester_z_get_product_name(&product_name);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_z_get_product_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Product name: %s", product_name);

	for (;;) {
		struct hio_chester_z_status status;

		ret = hio_chester_z_get_status(&status);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_get_status` failed: %d", ret);
		} else {
			LOG_INF("DC input connected: %d", (int)status.dc_input_connected);
			LOG_INF("Button 0 pressed: %d", (int)status.button_0_pressed);
			LOG_INF("Button 1 pressed: %d", (int)status.button_1_pressed);
			LOG_INF("Button 2 pressed: %d", (int)status.button_2_pressed);
			LOG_INF("Button 3 pressed: %d", (int)status.button_3_pressed);
			LOG_INF("Button 4 pressed: %d", (int)status.button_4_pressed);
		}

		uint16_t vdc;

		ret = hio_chester_z_get_vdc_mv(&vdc);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_get_vdc_mv` failed: %d", ret);
		} else {
			LOG_INF("VDC = %u mV", vdc);
		}

		uint16_t vbat;

		ret = hio_chester_z_get_vbat_mv(&vbat);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_z_get_vbat_mv` failed: %d", ret);
		} else {
			LOG_INF("VBAT = %u mV", vbat);
		}

		k_sleep(K_MSEC(1000));
	}

	return 0;
}
