/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static struct ctr_wdog_channel m_wdog_ch1;
static struct ctr_wdog_channel m_wdog_ch2;

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_wdog_set_timeout(10000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_install(&m_wdog_ch1);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed (m_wdog_ch2): %d", ret);
		k_oops();
	}

	ret = ctr_wdog_install(&m_wdog_ch2);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed (m_wdog_ch2): %d", ret);
		k_oops();
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(10));
		ctr_led_set(CTR_LED_CHANNEL_R, false);

#if 1
		ret = ctr_wdog_feed(&m_wdog_ch1);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed (m_wdog_ch1): %d", ret);
			k_oops();
		}
#endif

#if 0
		ret = ctr_wdog_feed(&m_wdog_ch2);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed (m_wdog_ch2): %d", ret);
			k_oops();
		}
#endif

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
