/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_led, LOG_LEVEL_DBG);

static const struct device *m_dev_led_r = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_r)));
static const struct device *m_dev_led_g = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_g)));
static const struct device *m_dev_led_y = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_y)));
static const struct device *m_dev_led_ext = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(led_ext)));
static const struct gpio_dt_spec m_dev_led_load = GPIO_DT_SPEC_GET(DT_NODELABEL(led_load), gpios);

int ctr_led_set(enum ctr_led_channel channel, bool is_on)
{
	int ret;

	const struct device *dev;
	uint32_t led;

	switch (channel) {
	case CTR_LED_CHANNEL_R:
		if (!device_is_ready(m_dev_led_r)) {
			LOG_ERR("Device `LED_R` not ready");
			return -EINVAL;
		}

		dev = m_dev_led_r;
		led = 0;

		break;

	case CTR_LED_CHANNEL_G:
		if (!device_is_ready(m_dev_led_g)) {
			LOG_ERR("Device `LED_G` not ready");
			return -EINVAL;
		}

		dev = m_dev_led_g;
		led = 1;

		break;

	case CTR_LED_CHANNEL_Y:
		if (!device_is_ready(m_dev_led_y)) {
			LOG_ERR("Device `LED_Y` not ready");
			return -EINVAL;
		}

		dev = m_dev_led_y;
		led = 2;

		break;

	case CTR_LED_CHANNEL_EXT:
		if (!device_is_ready(m_dev_led_ext)) {
			LOG_ERR("Device `LED_EXT` not ready");
			return -EINVAL;
		}

		dev = m_dev_led_ext;
		led = 0;
		break;

	// the load LED has different behaviour
	case CTR_LED_CHANNEL_LOAD:
		if (is_on) {
			ret = gpio_pin_configure_dt(&m_dev_led_load, GPIO_OUTPUT_ACTIVE);
			if (ret) {
				LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
				return ret;
			}
		} else {
			ret = gpio_pin_configure_dt(&m_dev_led_load, GPIO_DISCONNECTED);
			if (ret) {
				LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
				return ret;
			}
		}

		return 0;

	default:
		LOG_ERR("Invalid channel: %d", channel);
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

static int init(void)
{
	int ret = gpio_pin_configure_dt(&m_dev_led_load, GPIO_DISCONNECTED);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_LED_INIT_PRIORITY);
