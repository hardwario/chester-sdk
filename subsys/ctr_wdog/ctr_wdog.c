/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>

LOG_MODULE_REGISTER(ctr_wdog, CONFIG_CTR_WDOG_LOG_LEVEL);

static atomic_t m_lock;
static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));
static int m_timeout_msec = 0;

int ctr_wdog_set_timeout(int timeout_msec)
{
	if (atomic_get(&m_lock)) {
		LOG_ERR("Timeout already locked");
		return -EBUSY;
	}

	m_timeout_msec = timeout_msec;

	return 0;
}

int ctr_wdog_install(struct ctr_wdog_channel *channel)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	struct wdt_timeout_cfg cfg = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.min = 0,
		.window.max = m_timeout_msec,
	};

	atomic_set(&m_lock, true);

	ret = wdt_install_timeout(m_dev, &cfg);
	if (ret < 0) {
		LOG_ERR("Call `wdt_install_timeout` failed: %d", ret);
		return ret;
	}

	channel->id = ret;

	return 0;
}

int ctr_wdog_start(void)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	ret = wdt_setup(m_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
	if (ret) {
		LOG_ERR("Call `wdt_setup` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_wdog_feed(struct ctr_wdog_channel *channel)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		return -EINVAL;
	}

	ret = wdt_feed(m_dev, channel->id);
	if (ret) {
		LOG_ERR("Call `wdt_feed` failed: %d", ret);
		return ret;
	}

	return 0;
}
