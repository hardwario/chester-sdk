/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_a1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#define DT_DRV_COMPAT hardwario_ctr_a1

LOG_MODULE_REGISTER(ctr_a1, CONFIG_CTR_A1_LOG_LEVEL);

struct ctr_a1_config {
	const struct gpio_dt_spec relay1_spec;
	const struct gpio_dt_spec relay2_spec;
	const struct gpio_dt_spec led1_spec;
	const struct gpio_dt_spec led2_spec;
};

struct ctr_a1_data {
	const struct device *dev;
};

static inline const struct ctr_a1_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_a1_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_a1_set_relay_(const struct device *dev, enum ctr_a1_relay relay, bool is_on)
{
	int ret;

#define SET_RELAY(ch)                                                                              \
	do {                                                                                       \
		if (relay == CTR_A1_RELAY_##ch) {                                                  \
			if (!device_is_ready(get_config(dev)->relay##ch##_spec.port)) {            \
				LOG_ERR("Device not ready");                                       \
				return -ENODEV;                                                    \
			}                                                                          \
			ret = gpio_pin_set_dt(&get_config(dev)->relay##ch##_spec, is_on ? 1 : 0);  \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);                 \
				return ret;                                                        \
			}                                                                          \
		}                                                                                  \
	} while (0)

	SET_RELAY(1);
	SET_RELAY(2);

#undef SET_RELAY

	return 0;
}

static int ctr_a1_set_led_(const struct device *dev, enum ctr_a1_led led, bool is_on)
{
	int ret;

#define SET_LED(ch)                                                                                \
	do {                                                                                       \
		if (led == CTR_A1_LED_##ch) {                                                      \
			if (!device_is_ready(get_config(dev)->led##ch##_spec.port)) {              \
				LOG_ERR("Device not ready");                                       \
				return -ENODEV;                                                    \
			}                                                                          \
			ret = gpio_pin_set_dt(&get_config(dev)->led##ch##_spec, is_on ? 1 : 0);    \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);                 \
				return ret;                                                        \
			}                                                                          \
		}                                                                                  \
	} while (0)

	SET_LED(1);
	SET_LED(2);

#undef SET_LED

	return 0;
}

static int ctr_a1_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

#define CHECK_READY_RELAY(ch)                                                                      \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->relay##ch##_spec.port)) {                    \
			LOG_ERR("Device not ready");                                               \
			return -ENODEV;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY_RELAY(1);
	CHECK_READY_RELAY(2);

#undef CHECK_READY_RELAY

#define CHECK_READY_LED(ch)                                                                        \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->led##ch##_spec.port)) {                      \
			LOG_ERR("Device not ready");                                               \
			return -ENODEV;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY_LED(1);
	CHECK_READY_LED(2);

#undef CHECK_READY_LED_READY

#define CONFIGURE_RELAY(ch)                                                                        \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->relay##ch##_spec,                    \
					    GPIO_OUTPUT_INACTIVE);                                 \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	CONFIGURE_RELAY(1);
	CONFIGURE_RELAY(2);

#undef CONFIGURE_RELAY

#define CONFIGURE_LED(ch)                                                                          \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->led##ch##_spec,                      \
					    GPIO_OUTPUT_INACTIVE);                                 \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	CONFIGURE_LED(1);
	CONFIGURE_LED(2);

#undef CONFIGURE_LED

	return 0;
}

static const struct ctr_a1_driver_api ctr_a1_driver_api = {
	.set_relay = ctr_a1_set_relay_,
	.set_led = ctr_a1_set_led_,
};

#define CTR_A1_INIT(n)                                                                             \
	static const struct ctr_a1_config inst_##n##_config = {                                    \
		.relay1_spec = GPIO_DT_SPEC_INST_GET(n, relay1_gpios),                             \
		.relay2_spec = GPIO_DT_SPEC_INST_GET(n, relay2_gpios),                             \
		.led1_spec = GPIO_DT_SPEC_INST_GET(n, led1_gpios),                                 \
		.led2_spec = GPIO_DT_SPEC_INST_GET(n, led2_gpios),                                 \
	};                                                                                         \
	static struct ctr_a1_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_a1_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_A1_INIT_PRIORITY, &ctr_a1_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_A1_INIT)
