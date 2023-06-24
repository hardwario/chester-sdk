/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_b1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_b1

LOG_MODULE_REGISTER(ctr_b1, CONFIG_CTR_B1_LOG_LEVEL);

struct ctr_b1_config {
	const struct gpio_dt_spec wm_reset_spec;
	const struct gpio_dt_spec wm_on_spec;
	const struct gpio_dt_spec ant_1_spec;
	const struct gpio_dt_spec ant_2_spec;
};

struct ctr_b1_data {
	struct k_sem lock;
	const struct device *dev;
};

static inline const struct ctr_b1_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_b1_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_b1_set_output_(const struct device *dev, enum ctr_b1_output output, int value)
{
	int ret;

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

#define SET_OUTPUT(ch_upper, ch_lower)                                                             \
	do {                                                                                       \
		if (output == CTR_B1_OUTPUT_##ch_upper) {                                          \
			if (!device_is_ready(get_config(dev)->ch_lower##_spec.port)) {             \
				LOG_ERR("Device not ready");                                       \
				k_sem_give(&get_data(dev)->lock);                                  \
				return -ENODEV;                                                    \
			}                                                                          \
			ret = gpio_pin_set_dt(&get_config(dev)->ch_lower##_spec, value);           \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);                 \
				k_sem_give(&get_data(dev)->lock);                                  \
				return ret;                                                        \
			}                                                                          \
		}                                                                                  \
	} while (0)

	SET_OUTPUT(WM_RESET, wm_reset);
	SET_OUTPUT(WM_ON, wm_on);
	SET_OUTPUT(ANT_1, ant_1);
	SET_OUTPUT(ANT_2, ant_2);

#undef SET_OUTPUT

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_b1_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

#define CHECK_READY(name)                                                                          \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->name##_spec.port)) {                         \
			LOG_ERR("Device not ready");                                               \
			return -ENODEV;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY(wm_reset);
	CHECK_READY(wm_on);
	CHECK_READY(ant_1);
	CHECK_READY(ant_2);

#undef CHECK_READY

#define SETUP_OUTPUT(name)                                                                         \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->name##_spec, GPIO_OUTPUT_INACTIVE);  \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	SETUP_OUTPUT(wm_reset);
	SETUP_OUTPUT(wm_on);
	SETUP_OUTPUT(ant_1);
	SETUP_OUTPUT(ant_2);

#undef SETUP_OUTPUT

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static const struct ctr_b1_driver_api ctr_b1_driver_api = {
	.set_output = ctr_b1_set_output_,
};

#define CTR_B1_INIT(n)                                                                             \
	static const struct ctr_b1_config inst_##n##_config = {                                    \
		.wm_reset_spec = GPIO_DT_SPEC_INST_GET(n, wm_reset_gpios),                         \
		.wm_on_spec = GPIO_DT_SPEC_INST_GET(n, wm_on_gpios),                               \
		.ant_1_spec = GPIO_DT_SPEC_INST_GET(n, ant_1_gpios),                               \
		.ant_2_spec = GPIO_DT_SPEC_INST_GET(n, ant_2_gpios),                               \
	};                                                                                         \
	static struct ctr_b1_data inst_##n##_data = {                                              \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 0, 1),                             \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_b1_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_B1_INIT_PRIORITY, &ctr_b1_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_B1_INIT)

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_b1_sc16is740), okay)

BUILD_ASSERT(CONFIG_UART_SC16IS7XX_INIT_PRIORITY > CONFIG_GPIO_PCA953X_INIT_PRIORITY,
	     "CONFIG_UART_SC16IS7XX_INIT_PRIORITY must be higher than "
	     "CONFIG_GPIO_PCA953X_INIT_PRIORITY");

BUILD_ASSERT(CONFIG_CTR_B1_INIT_PRIORITY > CONFIG_UART_SC16IS7XX_INIT_PRIORITY,
	     "CONFIG_CTR_B1_INIT_PRIORITY must be higher than "
	     "CONFIG_UART_SC16IS7XX_INIT_PRIORITY");

#endif /* DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_b1_sc16is740), okay) */
