/* CHESTER includes */
#include <drivers/ctr_z.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

static const struct device *m_ctr_z_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

void ctr_z_event_handler(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	int ret;

	LOG_DBG("Event: %d", event);

	switch (event) {
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
		.pattern = CTR_Z_LED_PATTERN_NONE,
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

void main(void)
{
	int ret;

	ret = ctr_z_set_handler(m_ctr_z_dev, ctr_z_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
		k_oops();
	}

	uint32_t serial_number;
	ret = ctr_z_get_serial_number(m_ctr_z_dev, &serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_serial_number` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Serial number: %u", serial_number);

	uint16_t hw_revision;
	ret = ctr_z_get_hw_revision(m_ctr_z_dev, &hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_revision` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Hardware revision: 0x%04x", hw_revision);

	uint32_t hw_variant;
	ret = ctr_z_get_hw_variant(m_ctr_z_dev, &hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_variant` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Hardware variant: 0x%08x", hw_variant);

	uint32_t fw_version;
	ret = ctr_z_get_fw_version(m_ctr_z_dev, &fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
		k_oops();
	}

	LOG_INF("Firmware version: 0x%08x", fw_version);

	for (;;) {
		uint16_t vdc;
		ret = ctr_z_get_vdc_mv(m_ctr_z_dev, &vdc);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Voltage VDC: %u mV", vdc);

		uint16_t vbat;
		ret = ctr_z_get_vbat_mv(m_ctr_z_dev, &vbat);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Voltage VBAT: %u mV", vbat);

		k_sleep(K_MSEC(1000));
	}
}
