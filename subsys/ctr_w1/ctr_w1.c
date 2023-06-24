/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_w1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_w1, CONFIG_CTR_W1_LOG_LEVEL);

#define ACQUIRE_DELAY K_MSEC(3)

struct scan_item {
	struct w1_rom rom;
	sys_snode_t node;
};

int ctr_w1_acquire(struct ctr_w1 *w1, const struct device *dev)
{
	int ret;
	int res = 0;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = w1_lock_bus(dev);
	if (ret) {
		LOG_ERR("Call `w1_lock_bus` failed: %d", ret);
		res = ret;
		goto error;
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		res = ret;
		goto error;
	}

	/*
	 * TODO This should be part of the bus driver. The driver
	 * must guarantee nobody will access the I2C bus during the delay.
	 */
	k_sleep(ACQUIRE_DELAY);

	ret = w1_reset_bus(dev);
	if (ret < 0) {
		LOG_ERR("Call `w1_reset_bus` failed: %d", ret);
		res = ret;
		goto error;
	}

	return 0;

error:
	ret = w1_unlock_bus(dev);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		res = res ? res : ret;
	}

	return res;
}

int ctr_w1_release(struct ctr_w1 *w1, const struct device *dev)
{
	int ret;
	int res = 0;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	if (w1->is_ds28e17_present) {
		ret = w1_reset_bus(dev);
		if (ret == 1) {
			const struct w1_slave_config config = {.overdrive = 0};
			ret = w1_skip_rom(dev, &config);
			if (ret) {
				LOG_WRN("Call `w1_skip_rom` failed: %d", ret);
			} else {
				const uint8_t buf[] = {0x1e};
				ret = w1_write_block(dev, buf, sizeof(buf));
				if (ret) {
					LOG_WRN("Call `w1_write_block` failed: %d", ret);
				}
			}
		}
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		res = ret;
		goto error;
	}

error:
	ret = w1_unlock_bus(dev);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		res = res ? res : ret;
	}

	return res;
}

static void w1_search_callback(struct w1_rom rom, void *cb_arg)
{
	struct ctr_w1 *w1 = cb_arg;

	if (rom.family == 0x19) {
		w1->is_ds28e17_present = true;
	}

	struct scan_item *item = k_malloc(sizeof(*item));
	if (!item) {
		LOG_ERR("Call `k_malloc` failed");
		return;
	}

	item->rom = rom;

	sys_slist_append(&w1->scan_list, &item->node);
}

int ctr_w1_scan(struct ctr_w1 *w1, const struct device *dev,
		int (*user_cb)(struct w1_rom rom, void *user_data), void *user_data)
{
	int ret;
	int res = 0;

	w1->is_ds28e17_present = false;

	sys_slist_init(&w1->scan_list);

	ret = w1_search_rom(dev, w1_search_callback, w1);
	if (ret < 0) {
		LOG_ERR("Call `w1_search_rom` failed: %d", ret);
		res = ret;
		goto error;
	}

	LOG_DBG("Found %d device(s)", ret);

	struct scan_item *item;
	struct scan_item *item_safe;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE (&w1->scan_list, item, item_safe, node) {
		if (user_cb) {
			ret = user_cb(item->rom, user_data);
			if (ret) {
				LOG_ERR("Call `user_cb` failed: %d", ret);
			}
		}
	}

error:
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE (&w1->scan_list, item, item_safe, node) {
		sys_slist_remove(&w1->scan_list, NULL, &item->node);
		k_free(item);
	}

	return res;
}
