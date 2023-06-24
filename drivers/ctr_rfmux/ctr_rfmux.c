/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#define DT_DRV_COMPAT hardwario_ctr_rfmux

LOG_MODULE_REGISTER(ctr_rfmux, CONFIG_CTR_RFMUX_LOG_LEVEL);

struct ctr_rfmux_config {
	const struct gpio_dt_spec rf_lte_spec;
	const struct gpio_dt_spec rf_lrw_spec;
	const struct gpio_dt_spec rf_int_spec;
	const struct gpio_dt_spec rf_ext_spec;
};

struct ctr_rfmux_data {
	const struct device *dev;
	struct k_sem lock;
	bool is_acquired;
};

static inline const struct ctr_rfmux_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_rfmux_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_rfmux_acquire_(const struct device *dev)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->is_acquired) {
		k_sem_give(&get_data(dev)->lock);
		return -EBUSY;
	}

	get_data(dev)->is_acquired = true;

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_rfmux_release_(const struct device *dev)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->is_acquired) {
		k_sem_give(&get_data(dev)->lock);
		return -EACCES;
	}

	get_data(dev)->is_acquired = false;

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_rfmux_set_interface_(const struct device *dev, enum ctr_rfmux_interface interface)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->is_acquired) {
		k_sem_give(&get_data(dev)->lock);
		return -EACCES;
	}

	if (!device_is_ready(get_config(dev)->rf_lte_spec.port)) {
		LOG_ERR("Port `RF_LTE` not ready");
		k_sem_give(&get_data(dev)->lock);
		return -ENODEV;
	}

	if (!device_is_ready(get_config(dev)->rf_lrw_spec.port)) {
		LOG_ERR("Port `RF_LRW` not ready");
		k_sem_give(&get_data(dev)->lock);
		return -ENODEV;
	}

	switch (interface) {
	case CTR_RFMUX_INTERFACE_NONE:
		LOG_INF("Setting interface: NONE");

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lte_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_LTE): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lrw_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_LRW): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		break;

	case CTR_RFMUX_INTERFACE_LTE:
		LOG_INF("Setting interface: LTE");

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lrw_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_LRW): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lte_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_LTE): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		break;

	case CTR_RFMUX_INTERFACE_LRW:
		LOG_INF("Setting interface: LRW");

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lte_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_LTE): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lrw_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_LRW): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		break;

	default:
		LOG_ERR("Unknown interface: %d", interface);
		k_sem_give(&get_data(dev)->lock);
		return -EINVAL;
	}

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_rfmux_set_antenna_(const struct device *dev, enum ctr_rfmux_antenna antenna)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->is_acquired) {
		k_sem_give(&get_data(dev)->lock);
		return -EACCES;
	}

	if (!device_is_ready(get_config(dev)->rf_int_spec.port)) {
		LOG_ERR("Port `RF_INT` not ready");
		k_sem_give(&get_data(dev)->lock);
		return -ENODEV;
	}

	if (!device_is_ready(get_config(dev)->rf_ext_spec.port)) {
		LOG_ERR("Port `RF_EXT` not ready");
		k_sem_give(&get_data(dev)->lock);
		return -ENODEV;
	}

	switch (antenna) {
	case CTR_RFMUX_ANTENNA_NONE:
		LOG_INF("Setting antenna: NONE");

		ret = gpio_pin_set_dt(&get_config(dev)->rf_int_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_INT): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_ext_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_EXT): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		break;

	case CTR_RFMUX_ANTENNA_INT:
		LOG_INF("Setting antenna: INT");

		ret = gpio_pin_set_dt(&get_config(dev)->rf_ext_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_EXT): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_int_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_INT): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		break;

	case CTR_RFMUX_ANTENNA_EXT:
		LOG_INF("Setting antenna: EXT");

		ret = gpio_pin_set_dt(&get_config(dev)->rf_int_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_INT): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_ext_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed (RF_EXT): %d", ret);
			k_sem_give(&get_data(dev)->lock);
			return ret;
		}

		break;

	default:
		LOG_ERR("Unknown antenna: %d", antenna);
		k_sem_give(&get_data(dev)->lock);
		return -EINVAL;
	}

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_rfmux_init(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->rf_lte_spec.port)) {
		LOG_ERR("Port `RF_LTE` not ready");
		return -ENODEV;
	}

	if (!device_is_ready(get_config(dev)->rf_lrw_spec.port)) {
		LOG_ERR("Port `RF_LRW` not ready");
		return -ENODEV;
	}

	if (!device_is_ready(get_config(dev)->rf_int_spec.port)) {
		LOG_ERR("Port `RF_INT` not ready");
		return -ENODEV;
	}

	if (!device_is_ready(get_config(dev)->rf_ext_spec.port)) {
		LOG_ERR("Port `RF_EXT` not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_lte_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed (RF_LTE): %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_lrw_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed (RF_LRW): %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_int_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed (RF_INT): %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_ext_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed (RF_EXT): %d", ret);
		return ret;
	}

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static const struct ctr_rfmux_driver_api ctr_rfmux_driver_api = {
	.acquire = ctr_rfmux_acquire_,
	.release = ctr_rfmux_release_,
	.set_interface = ctr_rfmux_set_interface_,
	.set_antenna = ctr_rfmux_set_antenna_,
};

#define CTR_RFMUX_INIT(n)                                                                          \
	static const struct ctr_rfmux_config inst_##n##_config = {                                 \
		.rf_lte_spec = GPIO_DT_SPEC_INST_GET(n, rf_lte_gpios),                             \
		.rf_lrw_spec = GPIO_DT_SPEC_INST_GET(n, rf_lrw_gpios),                             \
		.rf_int_spec = GPIO_DT_SPEC_INST_GET(n, rf_int_gpios),                             \
		.rf_ext_spec = GPIO_DT_SPEC_INST_GET(n, rf_ext_gpios),                             \
	};                                                                                         \
	static struct ctr_rfmux_data inst_##n##_data = {                                           \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 0, 1),                             \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_rfmux_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
			      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                     \
			      &ctr_rfmux_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_RFMUX_INIT)
