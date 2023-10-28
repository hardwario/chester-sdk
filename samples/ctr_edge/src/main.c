/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_edge.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void edge_callback(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	LOG_INF("Event: %d", event);
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct gpio_dt_spec spec = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

	struct ctr_edge edge;

	ret = ctr_edge_init(&edge, &spec, false);
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
