/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_flash.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

/* Standard includes */
#include <errno.h>

LOG_MODULE_REGISTER(ctr_flash, CONFIG_CTR_FLASH_LOG_LEVEL);

static K_MUTEX_DEFINE(m_lock);

static int suspend(void)
{
	int ret;

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(spi_flash0));
	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int resume(void)
{
	int ret;

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(spi_flash0));
	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_flash_acquire(void)
{
	int ret;

	ret = k_mutex_lock(&m_lock, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_mutex_lock` failed: %d", ret);
		return ret;
	}

	ret = resume();
	if (ret) {
		LOG_ERR("Call `resume` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_flash_release(void)
{
	int ret;

	ret = suspend();
	if (ret) {
		LOG_ERR("Call `suspend` failed: %d", ret);
		return ret;
	}

	ret = k_mutex_unlock(&m_lock);
	if (ret) {
		LOG_ERR("Call `k_mutex_unlock` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	ret = suspend();
	if (ret) {
		LOG_ERR("Call `suspend` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_FLASH_INIT_PRIORITY);
