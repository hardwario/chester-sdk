#include <ctr_led.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/led.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_led, LOG_LEVEL_DBG);

static const struct device *m_dev_led_r = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_r)));
static const struct device *m_dev_led_g = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_g)));
static const struct device *m_dev_led_y = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_y)));
static const struct device *m_dev_led_ext = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_ext)));

static int set_led(enum ctr_led_channel channel, bool is_on)
{
	int ret;

	const struct device *dev;
	uint32_t led;

	switch (channel) {
	case CTR_LED_CHANNEL_R:
		dev = m_dev_led_r;
		led = 0;
		break;

	case CTR_LED_CHANNEL_G:
		dev = m_dev_led_g;
		led = 1;
		break;

	case CTR_LED_CHANNEL_Y:
		dev = m_dev_led_y;
		led = 2;
		break;

	case CTR_LED_CHANNEL_EXT:
		dev = m_dev_led_ext;
		led = 0;
		break;

	default:
		LOG_ERR("Invalid channel: %d", channel);
		return -EINVAL;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	if (is_on) {
		ret = led_on(dev, led);

		if (ret) {
			LOG_ERR("Call `led_on` failed: %d", ret);
			return ret;
		}

	} else {
		ret = led_off(dev, led);

		if (ret) {
			LOG_ERR("Call `led_off` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

int ctr_led_set(enum ctr_led_channel channel, bool is_on)
{
	int ret;

	ret = set_led(channel, is_on);

	if (ret) {
		LOG_ERR("Call `set_led` failed: %d", ret);
		return ret;
	}

	return 0;
}
