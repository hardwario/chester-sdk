/* CHESTER includes */
#include <drivers/ctr_s1.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

void ctr_s1_event_handler(const struct device *dev, enum ctr_s1_event event, void *user_data)
{
	int ret;
	int pir_motion_count;

	switch (event) {
	case CTR_S1_EVENT_DEVICE_RESET:
		ret = ctr_s1_apply(dev);
		if (ret) {
			LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
			k_oops();
		}
		return;

	case CTR_S1_EVENT_PIR_MOTION:
		ret = ctr_s1_pir_motion_read(dev, &pir_motion_count);
		LOG_INF("PIR count: %d", pir_motion_count);
		return;

	case CTR_S1_EVENT_BUTTON_PRESSED:
		LOG_INF("EVT button press");
		break;
	case CTR_S1_EVENT_BUTTON_HOLD:
		LOG_INF("EVT button hold - CO2 Calibration");
		ctr_s1_calibrate_target_co2_concentration(dev, 420);
		break;

	default:
		return;
	}

	static int led;

	struct ctr_s1_led_param param = {
		.brightness = CTR_S1_LED_BRIGHTNESS_HIGH,
		.command = CTR_S1_LED_COMMAND_1X_1_2,
		.pattern = CTR_S1_LED_PATTERN_NONE,
	};

	ret = ctr_s1_set_led(dev, led++ % 15, &param);
	if (ret) {
		LOG_ERR("Call `ctr_s1_set_led` failed: %d", ret);
		k_oops();
	}

	ret = ctr_s1_apply(dev);
	if (ret) {
		LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
		k_oops();
	}
}

void main(void)
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

		float s1_temperature;
		ret = ctr_s1_temperature_read(dev, &s1_temperature);
		if (ret) {
			LOG_ERR("Call `ctr_s1_temperature_read` failed: %d", ret);
		} else {
			LOG_INF("S1 temperature: %.1f C", s1_temperature);
		}

		float s1_humidity;
		ret = ctr_s1_humidity_read(dev, &s1_humidity);
		if (ret) {
			LOG_ERR("Call `ctr_s1_humidity_read` failed: %d", ret);
		} else {
			LOG_INF("S1 humidity: %.1f RH", s1_humidity);
		}

		float s1_co2_concentration;
		ret = ctr_s1_co2_concentration_read(dev, &s1_co2_concentration);
		if (ret) {
			LOG_ERR("Call `ctr_s1_co2_concentration_read` failed: %d", ret);
		} else {
			LOG_INF("S1 co2 concentration: %.0f ppm", s1_co2_concentration);
		}

		float s1_illuminance;
		ret = ctr_s1_illuminance_read(dev, &s1_illuminance);
		if (ret) {
			LOG_ERR("Call `ctr_s1_illuminance_read` failed: %d", ret);
		} else {
			LOG_INF("S1 illuminance: %.0f lux", s1_illuminance);
		}

		float s1_pressure;
		ret = ctr_s1_pressure_read(dev, &s1_pressure);
		if (ret) {
			LOG_ERR("Call `ctr_s1_pressure_read` failed: %d", ret);
		} else {
			LOG_INF("S1 pressure: %.0f Pa", s1_pressure);
		}

		float s1_altitude;
		ret = ctr_s1_altitude_read(dev, &s1_altitude);
		if (ret) {
			LOG_ERR("Call `ctr_s1_altitude_read` failed: %d", ret);
		} else {
			LOG_INF("S1 altitude: %.0f m", s1_altitude);
		}

		k_sleep(K_SECONDS(10));
	}
}
