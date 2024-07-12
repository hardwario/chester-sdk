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

struct ctr_rfmux_data {
	const struct device *dev;
	struct k_sem lock;
	bool is_acquired;
};

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

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->is_acquired) {
		k_sem_give(&get_data(dev)->lock);
		return -EACCES;
	}

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_rfmux_set_antenna_(const struct device *dev, enum ctr_rfmux_antenna antenna)
{

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->is_acquired) {
		k_sem_give(&get_data(dev)->lock);
		return -EACCES;
	}

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static int ctr_rfmux_init(const struct device *dev)
{

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
	static struct ctr_rfmux_data inst_##n##_data = {                                           \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 0, 1),                             \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_rfmux_init, NULL, &inst_##n##_data, NULL, POST_KERNEL,        \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ctr_rfmux_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_RFMUX_INIT)
