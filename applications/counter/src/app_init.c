#include "app_init.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_loop.h"

/* CHESTER includes */
#include <ctr_edge.h>
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_wdog.h>
#include <drivers/ctr_x0.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(g_app_init_sem, 0, 1);

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(CONFIG_SHIELD_CTR_X0_A)
static struct ctr_edge m_edge_ch1;
static struct ctr_edge m_edge_ch2;
static struct ctr_edge m_edge_ch3;
static struct ctr_edge m_edge_ch4;
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_X0_B)
static struct ctr_edge m_edge_ch5;
static struct ctr_edge m_edge_ch6;
static struct ctr_edge m_edge_ch7;
static struct ctr_edge m_edge_ch8;
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B)

#define EDGE_CALLBACK(ch)                                                                          \
	void edge_ch##ch##_callback(struct ctr_edge *edge, enum ctr_edge_event event,              \
	                            void *user_data)                                               \
	{                                                                                          \
		if (event == CTR_EDGE_EVENT_ACTIVE) {                                              \
			LOG_INF("Channel " STRINGIFY(ch) " detected active edge");                 \
			k_mutex_lock(&g_app_data_lock, K_FOREVER);                                 \
			g_app_data.counter_ch##ch++;                                               \
			k_mutex_unlock(&g_app_data_lock);                                          \
		}                                                                                  \
	}

#if defined(CONFIG_SHIELD_CTR_X0_A)
EDGE_CALLBACK(1)
EDGE_CALLBACK(2)
EDGE_CALLBACK(3)
EDGE_CALLBACK(4)
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_X0_B)
EDGE_CALLBACK(5)
EDGE_CALLBACK(6)
EDGE_CALLBACK(7)
EDGE_CALLBACK(8)
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

#undef EDGE_CALLBACK

static int init_chester_x0(void)
{
	int ret;

#define SETUP(dev, n, ch)                                                                          \
	do {                                                                                       \
		ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_##n, CTR_X0_MODE_NPN_INPUT);             \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);                         \
			return ret;                                                                \
		}                                                                                  \
		const struct gpio_dt_spec *spec;                                                   \
		ret = ctr_x0_get_spec(dev, CTR_X0_CHANNEL_##n, &spec);                             \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_x0_get_spec` failed: %d", ret);                         \
			return ret;                                                                \
		}                                                                                  \
		ret = gpio_pin_configure_dt(spec, GPIO_INPUT | GPIO_ACTIVE_LOW);                   \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_configure_dt` failed: %d", ret);                       \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_init(&m_edge_ch##ch, spec, false);                                  \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_init` failed: %d", ret);                           \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_callback(&m_edge_ch##ch, edge_ch##ch##_callback, NULL);         \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_cooldown_time(&m_edge_ch##ch, 10);                              \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);              \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_active_duration(&m_edge_ch##ch, 20);                            \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);            \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_inactive_duration(&m_edge_ch##ch, 20);                          \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);          \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_watch(&m_edge_ch##ch);                                              \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);                          \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

#if defined(CONFIG_SHIELD_CTR_X0_A)
	static const struct device *dev_a = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev_a)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	SETUP(dev_a, 1, 1);
	SETUP(dev_a, 2, 2);
	SETUP(dev_a, 3, 3);
	SETUP(dev_a, 4, 4);
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_X0_B)
	static const struct device *dev_b = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_b));

	if (!device_is_ready(dev_b)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	SETUP(dev_b, 1, 5);
	SETUP(dev_b, 2, 6);
	SETUP(dev_b, 3, 7);
	SETUP(dev_b, 4, 8);
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

#undef SETUP

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B) */

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

#if defined(CONFIG_SHIELD_CTR_X0_A) || defined(CONFIG_SHIELD_CTR_X0_B)
	ret = init_chester_x0();
	if (ret) {
		LOG_ERR("Call `init_chester_x0` failed: %d", ret);
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
