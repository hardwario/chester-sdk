/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x9.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#define DT_DRV_COMPAT hardwario_ctr_x9

LOG_MODULE_REGISTER(ctr_x9, CONFIG_CTR_X9_LOG_LEVEL);

struct ctr_x9_config {
	const struct gpio_dt_spec output1_spec;
	const struct gpio_dt_spec output2_spec;
	const struct gpio_dt_spec output3_spec;
	const struct gpio_dt_spec output4_spec;
};

struct ctr_x9_data {
	const struct device *dev;
};

static inline const struct ctr_x9_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x9_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x9_set_output_(const struct device *dev, enum ctr_x9_output output, bool is_on)
{
	int ret;

#define SET_OUTPUT(ch)                                                                             \
	do {                                                                                       \
		if (output == CTR_X9_OUTPUT_##ch) {                                                \
			if (!device_is_ready(get_config(dev)->output##ch##_spec.port)) {           \
				LOG_ERR("Device not ready");                                       \
				return -ENODEV;                                                    \
			}                                                                          \
			ret = gpio_pin_set_dt(&get_config(dev)->output##ch##_spec, is_on ? 1 : 0); \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);                 \
				return ret;                                                        \
			}                                                                          \
		}                                                                                  \
	} while (0)

	SET_OUTPUT(1);
	SET_OUTPUT(2);
	SET_OUTPUT(3);
	SET_OUTPUT(4);

#undef SET_OUTPUT

	return 0;
}

static int ctr_x9_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

#define CHECK_READY(ch)                                                                            \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->output##ch##_spec.port)) {                   \
			LOG_ERR("Device not ready");                                               \
			return -ENODEV;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY(1);
	CHECK_READY(2);
	CHECK_READY(3);
	CHECK_READY(4);

#undef CHECK_READY

#define CONFIGURE(ch)                                                                              \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->output##ch##_spec,                   \
					    GPIO_OUTPUT_INACTIVE);                                 \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	CONFIGURE(1);
	CONFIGURE(2);
	CONFIGURE(3);
	CONFIGURE(4);

#undef CONFIGURE

	return 0;
}

static const struct ctr_x9_driver_api ctr_x9_driver_api = {
	.set_output = ctr_x9_set_output_,
};

#define CTR_X9_INIT(n)                                                                             \
	static const struct ctr_x9_config inst_##n##_config = {                                    \
		.output1_spec = GPIO_DT_SPEC_INST_GET(n, output1_gpios),                           \
		.output2_spec = GPIO_DT_SPEC_INST_GET(n, output2_gpios),                           \
		.output3_spec = GPIO_DT_SPEC_INST_GET(n, output3_gpios),                           \
		.output4_spec = GPIO_DT_SPEC_INST_GET(n, output4_gpios),                           \
	};                                                                                         \
	static struct ctr_x9_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_x9_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_X9_INIT_PRIORITY, &ctr_x9_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X9_INIT)
