#include <ctr_gpio.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(ctr_gpio, CONFIG_CTR_GPIO_LOG_LEVEL);

static const struct device *m_gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));

int ctr_gpio_configure(enum ctr_gpio_channel channel, enum ctr_gpio_mode mode)
{
	int ret;

	if (!device_is_ready(m_gpio_dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
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

	ret = gpio_pin_configure(m_gpio_dev, (gpio_pin_t)channel, flags);
	if (ret) {
		LOG_ERR("Failed configuring pin %d: %d", channel, ret);
		return ret;
	}

	return 0;
}

int ctr_gpio_read(enum ctr_gpio_channel channel, int *value)
{
	int ret;

	if (!device_is_ready(m_gpio_dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	ret = gpio_pin_get(m_gpio_dev, (gpio_pin_t)channel);
	if (ret != 0 && ret != 1) {
		LOG_ERR("Failed reading pin %d: %d", channel, ret);
		return ret;
	}

	*value = ret;

	return 0;
}

int ctr_gpio_write(enum ctr_gpio_channel channel, int value)
{
	int ret;

	if (!device_is_ready(m_gpio_dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	ret = gpio_pin_set(m_gpio_dev, (gpio_pin_t)channel, value);
	if (ret) {
		LOG_ERR("Failed writing pin %d: %d", channel, ret);
		return ret;
	}

	return 0;
}
