/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_gpio.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ctr_gpio, CONFIG_CTR_GPIO_LOG_LEVEL);

static const struct gpio_dt_spec m_specs[] = {
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), a0_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), a1_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), a2_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), a3_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), b0_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), b1_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), b2_gpios),
	GPIO_DT_SPEC_GET(DT_NODELABEL(ctr_gpio), b3_gpios),
};

int ctr_gpio_set_mode(enum ctr_gpio_channel channel, enum ctr_gpio_mode mode)
{
	int ret;

	if (channel < 0 || channel >= ARRAY_SIZE(m_specs)) {
		LOG_ERR("Invalid channel: %d", channel);
		return -EINVAL;
	}

	const struct gpio_dt_spec spec = m_specs[channel];

	if (!device_is_ready(spec.port)) {
		LOG_ERR("Port not ready (channel: %d)", channel);
		return -ENODEV;
	}

	gpio_flags_t flags;

	switch (mode) {
	case CTR_GPIO_MODE_ANALOG:
		flags = GPIO_DISCONNECTED;
		break;
	case CTR_GPIO_MODE_OUTPUT:
		flags = GPIO_OUTPUT;
		break;
	case CTR_GPIO_MODE_OUTPUT_OD:
		flags = GPIO_OUTPUT | GPIO_OPEN_DRAIN;
		break;
	case CTR_GPIO_MODE_INPUT:
		flags = GPIO_INPUT;
		break;
	case CTR_GPIO_MODE_INPUT_PU:
		flags = GPIO_INPUT | GPIO_PULL_UP;
		break;
	case CTR_GPIO_MODE_INPUT_PD:
		flags = GPIO_INPUT | GPIO_PULL_DOWN;
		break;
	default:
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&spec, flags);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` (%d) failed: %d", channel, ret);
		return ret;
	}

	return 0;
}

int ctr_gpio_read(enum ctr_gpio_channel channel, int *value)
{
	int ret;

	if (channel < 0 || channel >= ARRAY_SIZE(m_specs)) {
		LOG_ERR("Invalid channel: %d", channel);
		return -EINVAL;
	}

	const struct gpio_dt_spec spec = m_specs[channel];

	if (!device_is_ready(spec.port)) {
		LOG_ERR("Port not ready (channel: %d)", channel);
		return -ENODEV;
	}

	ret = gpio_pin_get_dt(&spec);
	if (ret != 0 && ret != 1) {
		LOG_ERR("Call `gpio_pin_get_dt` (%d) failed: %d", channel, ret);
		return ret;
	}

	*value = ret;

	return 0;
}

int ctr_gpio_write(enum ctr_gpio_channel channel, int value)
{
	int ret;

	if (channel < 0 || channel >= ARRAY_SIZE(m_specs)) {
		LOG_ERR("Invalid channel: %d", channel);
		return -EINVAL;
	}

	const struct gpio_dt_spec spec = m_specs[channel];

	if (!device_is_ready(spec.port)) {
		LOG_ERR("Port not ready (channel: %d)", channel);
		return -ENODEV;
	}

	ret = gpio_pin_set_dt(&spec, value);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` (%d) failed: %d", channel, ret);
		return ret;
	}

	return 0;
}
