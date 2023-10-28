/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_edge.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Example showing how to use CHESTER-X2 EN pin change to trigger a callback. */

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void edge_callback(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	LOG_INF("Event: %d", event);
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct gpio_dt_spec en_spec = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), en_gpios);

	if (!device_is_ready(en_spec.port)) {
		LOG_ERR("Port not ready");
		k_oops();
	}

	ret = gpio_pin_configure_dt(&en_spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		k_oops();
	}

	struct ctr_edge edge;

	ret = ctr_edge_init(&edge, &en_spec, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_edge_set_callback(&edge, edge_callback, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);
		k_oops();
	}

	ret = ctr_edge_set_cooldown_time(&edge, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);
		k_oops();
	}

	ret = ctr_edge_set_active_duration(&edge, 50);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);
		k_oops();
	}

	ret = ctr_edge_set_inactive_duration(&edge, 50);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);
		k_oops();
	}

	ret = ctr_edge_watch(&edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
