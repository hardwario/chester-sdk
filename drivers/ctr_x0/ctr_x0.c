/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>

#define DT_DRV_COMPAT hardwario_ctr_x0

LOG_MODULE_REGISTER(ctr_x0, CONFIG_CTR_X0_LOG_LEVEL);

struct ctr_x0_config {
	const struct gpio_dt_spec ch1_spec;
	const struct gpio_dt_spec on1_spec;
	const struct gpio_dt_spec pu1_spec;
	const struct gpio_dt_spec pd1_spec;
	const struct gpio_dt_spec cl1_spec;
	const struct gpio_dt_spec ch2_spec;
	const struct gpio_dt_spec on2_spec;
	const struct gpio_dt_spec pu2_spec;
	const struct gpio_dt_spec pd2_spec;
	const struct gpio_dt_spec cl2_spec;
	const struct gpio_dt_spec ch3_spec;
	const struct gpio_dt_spec on3_spec;
	const struct gpio_dt_spec pu3_spec;
	const struct gpio_dt_spec pd3_spec;
	const struct gpio_dt_spec cl3_spec;
	const struct gpio_dt_spec ch4_spec;
	const struct gpio_dt_spec on4_spec;
	const struct gpio_dt_spec pu4_spec;
	const struct gpio_dt_spec pd4_spec;
	const struct gpio_dt_spec cl4_spec;
};

struct ctr_x0_data {
	const struct device *dev;
	enum ctr_x0_mode mode_1;
	enum ctr_x0_mode mode_2;
	enum ctr_x0_mode mode_3;
	enum ctr_x0_mode mode_4;
};

static inline const struct ctr_x0_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x0_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x0_set_mode_(const struct device *dev, enum ctr_x0_channel channel,
			    enum ctr_x0_mode mode)
{
	int ret;

#define SET_CHANNEL(ch)                                                                            \
	do {                                                                                       \
		if (channel == CTR_X0_CHANNEL_##ch) {                                              \
			if (!device_is_ready(get_config(dev)->on##ch##_spec.port)) {               \
				LOG_ERR("Device not ready");                                       \
				return -EINVAL;                                                    \
			}                                                                          \
			if (mode == get_data(dev)->mode_##ch) {                                    \
				break;                                                             \
			}                                                                          \
			ret = gpio_pin_configure_dt(&get_config(dev)->on##ch##_spec, GPIO_INPUT);  \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);           \
				return ret;                                                        \
			}                                                                          \
			ret = gpio_pin_configure_dt(&get_config(dev)->pu##ch##_spec, GPIO_INPUT);  \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);           \
				return ret;                                                        \
			}                                                                          \
			ret = gpio_pin_configure_dt(&get_config(dev)->pd##ch##_spec, GPIO_INPUT);  \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);           \
				return ret;                                                        \
			}                                                                          \
			ret = gpio_pin_configure_dt(&get_config(dev)->cl##ch##_spec, GPIO_INPUT);  \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);           \
				return ret;                                                        \
			}                                                                          \
			switch (mode) {                                                            \
			case CTR_X0_MODE_DEFAULT:                                                  \
				break;                                                             \
			case CTR_X0_MODE_NPN_INPUT:                                                \
				ret = gpio_pin_configure_dt(&get_config(dev)->pu##ch##_spec,       \
							    GPIO_OUTPUT_ACTIVE);                   \
				if (ret) {                                                         \
					LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);   \
					return ret;                                                \
				}                                                                  \
				break;                                                             \
			case CTR_X0_MODE_PNP_INPUT:                                                \
				ret = gpio_pin_configure_dt(&get_config(dev)->pd##ch##_spec,       \
							    GPIO_OUTPUT_ACTIVE);                   \
				if (ret) {                                                         \
					LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);   \
					return ret;                                                \
				}                                                                  \
				break;                                                             \
			case CTR_X0_MODE_AI_INPUT:                                                 \
				ret = gpio_pin_configure_dt(&get_config(dev)->pd##ch##_spec,       \
							    GPIO_OUTPUT_ACTIVE);                   \
				if (ret) {                                                         \
					LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);   \
					return ret;                                                \
				}                                                                  \
				break;                                                             \
			case CTR_X0_MODE_CL_INPUT:                                                 \
				ret = gpio_pin_configure_dt(&get_config(dev)->cl##ch##_spec,       \
							    GPIO_OUTPUT_ACTIVE);                   \
				if (ret) {                                                         \
					LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);   \
					return ret;                                                \
				}                                                                  \
				ret = gpio_pin_configure_dt(&get_config(dev)->pd##ch##_spec,       \
							    GPIO_OUTPUT_ACTIVE);                   \
				if (ret) {                                                         \
					LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);   \
					return ret;                                                \
				}                                                                  \
				break;                                                             \
			case CTR_X0_MODE_PWR_SOURCE:                                               \
				ret = gpio_pin_configure_dt(&get_config(dev)->on##ch##_spec,       \
							    GPIO_OUTPUT_ACTIVE);                   \
				if (ret) {                                                         \
					LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);   \
					return ret;                                                \
				}                                                                  \
				break;                                                             \
			default:                                                                   \
				LOG_ERR("Unknown mode: %d", mode);                                 \
				return -EINVAL;                                                    \
			}                                                                          \
			get_data(dev)->mode_##ch = mode;                                           \
		}                                                                                  \
	} while (0)

	SET_CHANNEL(1);
	SET_CHANNEL(2);
	SET_CHANNEL(3);
	SET_CHANNEL(4);

#undef SET_CHANNEL

	return 0;
}

static int ctr_x0_get_spec_(const struct device *dev, enum ctr_x0_channel channel,
			    const struct gpio_dt_spec **spec)
{
	switch (channel) {
	case CTR_X0_CHANNEL_1:
		*spec = &get_config(dev)->ch1_spec;
		break;
	case CTR_X0_CHANNEL_2:
		*spec = &get_config(dev)->ch2_spec;
		break;
	case CTR_X0_CHANNEL_3:
		*spec = &get_config(dev)->ch3_spec;
		break;
	case CTR_X0_CHANNEL_4:
		*spec = &get_config(dev)->ch4_spec;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	return 0;
}

static int ctr_x0_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

#define CHECK_READY(name)                                                                          \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->name##_spec.port)) {                         \
			LOG_ERR("Device not ready");                                               \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY(on1);
	CHECK_READY(pu1);
	CHECK_READY(pd1);
	CHECK_READY(cl1);
	CHECK_READY(on2);
	CHECK_READY(pu2);
	CHECK_READY(pd2);
	CHECK_READY(cl2);
	CHECK_READY(on3);
	CHECK_READY(pu3);
	CHECK_READY(pd3);
	CHECK_READY(cl3);
	CHECK_READY(on4);
	CHECK_READY(pu4);
	CHECK_READY(pd4);
	CHECK_READY(cl4);

#undef CHECK_READY

#define SETUP_PIN(name)                                                                            \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->name##_spec, GPIO_INPUT);            \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	SETUP_PIN(on1);
	SETUP_PIN(pu1);
	SETUP_PIN(pd1);
	SETUP_PIN(cl1);
	SETUP_PIN(on2);
	SETUP_PIN(pu2);
	SETUP_PIN(pd2);
	SETUP_PIN(cl2);
	SETUP_PIN(on3);
	SETUP_PIN(pu3);
	SETUP_PIN(pd3);
	SETUP_PIN(cl3);
	SETUP_PIN(on4);
	SETUP_PIN(pu4);
	SETUP_PIN(pd4);
	SETUP_PIN(cl4);

#undef SETUP_PIN

	return 0;
}

static const struct ctr_x0_driver_api ctr_x0_driver_api = {
	.set_mode = ctr_x0_set_mode_,
	.get_spec = ctr_x0_get_spec_,
};

#define CTR_X0_INIT(n)                                                                             \
	static const struct ctr_x0_config inst_##n##_config = {                                    \
		.ch1_spec = GPIO_DT_SPEC_INST_GET(n, ch1_gpios),                                   \
		.on1_spec = GPIO_DT_SPEC_INST_GET(n, on1_gpios),                                   \
		.pu1_spec = GPIO_DT_SPEC_INST_GET(n, pu1_gpios),                                   \
		.pd1_spec = GPIO_DT_SPEC_INST_GET(n, pd1_gpios),                                   \
		.cl1_spec = GPIO_DT_SPEC_INST_GET(n, cl1_gpios),                                   \
		.ch2_spec = GPIO_DT_SPEC_INST_GET(n, ch2_gpios),                                   \
		.on2_spec = GPIO_DT_SPEC_INST_GET(n, on2_gpios),                                   \
		.pu2_spec = GPIO_DT_SPEC_INST_GET(n, pu2_gpios),                                   \
		.pd2_spec = GPIO_DT_SPEC_INST_GET(n, pd2_gpios),                                   \
		.cl2_spec = GPIO_DT_SPEC_INST_GET(n, cl2_gpios),                                   \
		.ch3_spec = GPIO_DT_SPEC_INST_GET(n, ch3_gpios),                                   \
		.on3_spec = GPIO_DT_SPEC_INST_GET(n, on3_gpios),                                   \
		.pu3_spec = GPIO_DT_SPEC_INST_GET(n, pu3_gpios),                                   \
		.pd3_spec = GPIO_DT_SPEC_INST_GET(n, pd3_gpios),                                   \
		.cl3_spec = GPIO_DT_SPEC_INST_GET(n, cl3_gpios),                                   \
		.ch4_spec = GPIO_DT_SPEC_INST_GET(n, ch4_gpios),                                   \
		.on4_spec = GPIO_DT_SPEC_INST_GET(n, on4_gpios),                                   \
		.pu4_spec = GPIO_DT_SPEC_INST_GET(n, pu4_gpios),                                   \
		.pd4_spec = GPIO_DT_SPEC_INST_GET(n, pd4_gpios),                                   \
		.cl4_spec = GPIO_DT_SPEC_INST_GET(n, cl4_gpios),                                   \
	};                                                                                         \
	static struct ctr_x0_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_x0_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_X0_INIT_PRIORITY, &ctr_x0_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X0_INIT)
