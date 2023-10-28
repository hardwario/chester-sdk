/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_gnss.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void gnss_handler(enum ctr_gnss_event event, union ctr_gnss_event_data *data, void *user_data)
{
	if (event == CTR_GNSS_EVENT_UPDATE) {
		LOG_DBG("Fix quality: %d", data->update.fix_quality);
		LOG_DBG("Satellites tracked: %d", data->update.satellites_tracked);
		LOG_DBG("Latitude: %.7f", data->update.latitude);
		LOG_DBG("Longitude: %.7f", data->update.longitude);
		LOG_DBG("Altitude: %.1f", data->update.altitude);
	}
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_gnss_set_handler(gnss_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_set_handler` failed: %d", ret);
		k_oops();
	}

	ret = ctr_gnss_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_start` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
