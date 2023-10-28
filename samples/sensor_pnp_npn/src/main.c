/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_edge.h>
#include <chester/ctr_led.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static struct ctr_edge m_edge_pnp;
static struct ctr_edge m_edge_npn;

static int m_pnp_activation_count;
static int m_pnp_deactivation_count;

static int m_npn_activation_count;
static int m_npn_deactivation_count;

void edge_callback(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	if (edge == &m_edge_pnp) {
		if (event == CTR_EDGE_EVENT_ACTIVE) {
			LOG_INF("PNP activation (count: %d)", ++m_pnp_activation_count);
			ctr_led_set(CTR_LED_CHANNEL_R, true);
		} else if (event == CTR_EDGE_EVENT_INACTIVE) {
			LOG_INF("PNP deactivation (count: %d)", ++m_pnp_deactivation_count);
			ctr_led_set(CTR_LED_CHANNEL_R, false);
		}

		return;
	}

	if (edge == &m_edge_npn) {
		if (event == CTR_EDGE_EVENT_ACTIVE) {
			LOG_INF("NPN activation (count: %d)", ++m_npn_activation_count);
			ctr_led_set(CTR_LED_CHANNEL_G, true);
		} else if (event == CTR_EDGE_EVENT_INACTIVE) {
			LOG_INF("NPN deactivation (count: %d)", ++m_npn_deactivation_count);
			ctr_led_set(CTR_LED_CHANNEL_G, false);
		}

		return;
	}
}

static int init_chester_x0(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

#define SETUP(ch, edge, mode, active)                                                              \
	do {                                                                                       \
		ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_##ch, mode);                             \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);                         \
			return ret;                                                                \
		}                                                                                  \
		const struct gpio_dt_spec *spec;                                                   \
		ret = ctr_x0_get_spec(dev, CTR_X0_CHANNEL_##ch, &spec);                            \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_x0_get_spec` failed: %d", ret);                         \
			return ret;                                                                \
		}                                                                                  \
		ret = gpio_pin_configure_dt(spec, GPIO_INPUT | active);                            \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_init(edge, spec, false);                                            \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_init` failed: %d", ret);                           \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_callback(edge, edge_callback, STRINGIFY(ch));                   \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_cooldown_time(edge, 10);                                        \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);              \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_active_duration(edge, 50);                                      \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);            \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_set_inactive_duration(edge, 50);                                    \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);          \
			return ret;                                                                \
		}                                                                                  \
		ret = ctr_edge_watch(edge);                                                        \
		if (ret) {                                                                         \
			LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);                          \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	SETUP(1, &m_edge_pnp, CTR_X0_MODE_PNP_INPUT, GPIO_ACTIVE_HIGH);
	SETUP(2, &m_edge_npn, CTR_X0_MODE_NPN_INPUT, GPIO_ACTIVE_LOW);

	return 0;
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = init_chester_x0();
	if (ret) {
		LOG_ERR("Call `init_chester_x0` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");
		k_sleep(K_SECONDS(30));
	}

	return 0;
}
