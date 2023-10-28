/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_edge.h>
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED_DELAY K_MSEC(500)

static void edge_callback(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	LOG_INF("%p: Event: %d", edge, event);

	ctr_led_set(CTR_LED_CHANNEL_EXT, event == CTR_EDGE_EVENT_ACTIVE ? true : false);
}

static int init_edge(void)
{
	int ret;

	static const struct gpio_dt_spec spec = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

	ret = gpio_pin_configure_dt(&spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	static struct ctr_edge edge;
	ret = ctr_edge_init(&edge, &spec, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(&edge, edge_callback, STRINGIFY(ch));
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(&edge, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(&edge, 50);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(&edge, 50);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_watch(&edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);
		return ret;
	}

	return 0;
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = init_edge();
	if (ret) {
		LOG_ERR("Call `init_edge` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		const enum ctr_led_channel channels[] = {
			CTR_LED_CHANNEL_R,
			CTR_LED_CHANNEL_G,
			CTR_LED_CHANNEL_Y,
		};

		for (size_t i = 0; i < 2 * ARRAY_SIZE(channels); i++) {
			ctr_led_set(channels[i / 2], i % 2 ? false : true);
			k_sleep(LED_DELAY);
		}
	}

	return 0;
}
