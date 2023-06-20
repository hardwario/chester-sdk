#include "app_data.h"
#include "app_counter.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_loop.h"

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(g_app_init_sem, 0, 1);

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(CONFIG_SHIELD_CTR_Z)
static int init_ctr_z(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_z_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_enable_interrupts` failed: %d", ret);
		return ret;
	}

	return 0;
}
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

int app_init(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_install(&g_app_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_SHIELD_CTR_Z)
	ret = init_ctr_z();
	if (ret) {
		LOG_ERR("Call `init_ctr_z` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B)
	ret = app_counter_init();
	if (ret) {
		LOG_ERR("Call `app_counter_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B) */

	ret = ctr_lte_set_event_cb(app_handler_lte, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		return ret;
	}

	for (;;) {
		ret = ctr_wdog_feed(&g_app_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			return ret;
		}

		ret = k_sem_take(&g_app_init_sem, K_SECONDS(1));
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			return ret;
		}

		break;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	return 0;
}
