/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <chester/ctr_lrw.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

static void lrw_event_cb(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param)
{
	int ret;

	LOG_DBG("ctr_lrw_event: %d, ctr_lrw_event_data: %p, param: %p", event, data, param);

	switch (event) {
	case CTR_LRW_EVENT_FAILURE:
		ret = ctr_lrw_start(NULL);
		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
			k_oops();
		}
		break;
	case CTR_LRW_EVENT_START_OK:
		ret = ctr_lrw_join(NULL);
		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_join` failed: %d", ret);
			k_oops();
		}
		break;
	default:
		LOG_DBG("unknown event: %d", event);
		break;
	}
}

int main(void)
{
	int ret;

	LOG_INF("LRW Example");

	ret = ctr_lrw_init(lrw_event_cb, NULL);
	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_start(NULL);
	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		k_oops();
	}

	struct ctr_lrw_send_opts lrw_send_opts = CTR_LRW_SEND_OPTS_DEFAULTS;
	int counter = 0;

	while (1) {
		LOG_DBG("Alive");

		ret = ctr_lrw_send(&lrw_send_opts, &counter, sizeof(counter), NULL);
		if (ret < 0) {
			LOG_WRN("Call `ctr_lrw_send` failed: %d", ret);
		}

		counter++;

		k_sleep(K_SECONDS(30));
	}

	return 0;
}
