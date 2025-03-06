/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "minmea.h"
#include "ctr_gnss_m8.h"

/* CHESTER includes */
#include <chester/ctr_gnss.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_info.h>
#include <chester/drivers/m8.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define K_MSGQ_DEFINE_STATIC(q_name, q_msg_size, q_max_msgs, q_align)                              \
	static char __noinit __aligned(q_align) _k_fifo_buf_##q_name[(q_max_msgs) * (q_msg_size)]; \
	static STRUCT_SECTION_ITERABLE(k_msgq, q_name) =                                           \
		Z_MSGQ_INITIALIZER(q_name, _k_fifo_buf_##q_name, q_msg_size, q_max_msgs)

LOG_MODULE_REGISTER(ctr_gnss, CONFIG_CTR_GNSS_LOG_LEVEL);

#define CMD_MSGQ_MAX_ITEMS 16

enum cmd_msgq_req {
	CMD_MSGQ_REQ_START = 0,
	CMD_MSGQ_REQ_STOP = 1,
};

struct cmd_msgq_data_stop {
	bool keep_bckp_domain;
};

union cmd_msgq_data {
	struct cmd_msgq_data_stop stop;
};

struct cmd_msgq_item {
	int corr_id;
	enum cmd_msgq_req req;
	union cmd_msgq_data data;
};

static ctr_gnss_user_cb m_user_cb;
static void *m_user_data;
static atomic_t m_corr_id;
static bool m_running;
enum ctr_info_product_family m_product_family;

K_MSGQ_DEFINE_STATIC(m_cmd_msgq, sizeof(struct cmd_msgq_item), CMD_MSGQ_MAX_ITEMS, 4);
#if defined(CONFIG_CTR_LTE_V2_GNSS)
K_MSGQ_DEFINE_STATIC(m_lte_v2_msgq, sizeof(struct ctr_lte_v2_gnss_update), 2, 1);
static void lte_v2_gnss_handler(const struct ctr_lte_v2_gnss_update *update, void *user_data)
{
	int ret = k_msgq_put(&m_lte_v2_msgq, update, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
	}
}
#endif

static int process_req_start(const struct cmd_msgq_item *item)
{
	int ret;

	ret = ctr_info_get_product_family(&m_product_family);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_product_family` failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_CTR_GNSS_M8)
	if (m_product_family == CTR_INFO_PRODUCT_FAMILY_CHESTER_M) {
		ctr_gnss_m8_set_handler(m_user_cb, m_user_data);
		return ctr_gnss_m8_start();
	}
#endif

#if defined(CONFIG_CTR_LTE_V2_GNSS)
	if (m_product_family == CTR_INFO_PRODUCT_FAMILY_CHESTER_U1) {
		ctr_lte_v2_gnss_set_handler(lte_v2_gnss_handler, NULL);
		return ctr_lte_v2_gnss_set_enable(true);
	}
#endif
	LOG_ERR("Unsupported product family: 0x%03x", m_product_family);

	return -ENOTSUP;
}

static int process_req_stop(const struct cmd_msgq_item *item)
{
	int ret = -ENOTSUP;

#if defined(CONFIG_CTR_GNSS_M8)
	if (m_product_family == CTR_INFO_PRODUCT_FAMILY_CHESTER_M) {
		ret = ctr_gnss_m8_stop(item->data.stop.keep_bckp_domain);
	}
#endif

#if defined(CONFIG_CTR_LTE_V2_GNSS)
	if (m_product_family == CTR_INFO_PRODUCT_FAMILY_CHESTER_U1) {
		ret = ctr_lte_v2_gnss_set_enable(false);
	}
#endif

	return ret;
}

static void process_cmd_msgq(void)
{
	int ret;

	struct cmd_msgq_item item;

	ret = k_msgq_get(&m_cmd_msgq, &item, m_running ? K_MSEC(100) : K_FOREVER);
	if (ret == -EAGAIN) {
		return;

	} else if (ret) {
		LOG_ERR("Call `k_msgq_get` failed: %d", ret);
		k_sleep(K_SECONDS(1));
		return;
	}

	if (item.req == CMD_MSGQ_REQ_START) {
		LOG_INF("Dequeued START command (correlation id: %d)", item.corr_id);

		if (m_running) {
			LOG_WRN("No reason for START operation - ignoring");

		} else {

			ret = process_req_start(&item);
			union ctr_gnss_event_data data = {0};

			if (ret) {
				LOG_ERR("Call `process_req_start` failed: %d", ret);

				if (m_user_cb) {
					data.start_err.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_START_ERR, &data, m_user_data);
				}

			} else {
				m_running = true;

				if (m_user_cb) {
					data.start_ok.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_START_OK, &data, m_user_data);
				}
			}
		}

	} else if (item.req == CMD_MSGQ_REQ_STOP) {
		if (!m_running) {
			LOG_WRN("No reason for STOP operation - ignoring");

		} else {
			ret = process_req_stop(&item);

			union ctr_gnss_event_data data = {0};

			if (ret) {
				LOG_ERR("Call `process_req_stop` failed: %d", ret);

				if (m_user_cb) {
					data.stop_err.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_STOP_ERR, &data, m_user_data);
				}

			} else {
				m_running = false;

				if (m_user_cb) {
					data.stop_ok.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_STOP_OK, &data, m_user_data);
				}
			}
		}
	}
}

static void dispatcher_thread(void)
{
	for (;;) {
		if (m_running) {
			int ret;

#if defined(CONFIG_CTR_GNSS_M8)
			if (m_product_family == CTR_INFO_PRODUCT_FAMILY_CHESTER_M) {
				ret = ctr_gnss_m8_process_data();
				if (ret) {
					LOG_ERR("Call `ctr_gnss_m8_process_data` failed: %d", ret);
				}
			}
#endif

#if defined(CONFIG_CTR_LTE_V2_GNSS)
			if (m_product_family == CTR_INFO_PRODUCT_FAMILY_CHESTER_U1) {
				struct ctr_lte_v2_gnss_update update;
				ret = k_msgq_get(&m_lte_v2_msgq, &update, K_MSEC(100));
				if (!ret) {
					union ctr_gnss_event_data data = {0};
					data.update.fix_quality = -1;
					data.update.satellites_tracked = -1;
					data.update.latitude = update.latitude;
					data.update.longitude = update.longitude;
					data.update.altitude = update.altitude;
					if (m_user_cb) {
						m_user_cb(CTR_GNSS_EVENT_UPDATE, &data,
							  m_user_data);
					}
				}
			}
#endif
		}

		process_cmd_msgq();
	}
}

K_THREAD_DEFINE(ctr_gnss, CONFIG_CTR_GNSS_THREAD_STACK_SIZE, dispatcher_thread, NULL, NULL, NULL,
		CONFIG_CTR_GNSS_THREAD_PRIORITY, 0, 0);

int ctr_gnss_set_handler(ctr_gnss_user_cb user_cb, void *user_data)
{
	m_user_cb = user_cb;
	m_user_data = user_data;

	return 0;
}

int ctr_gnss_start(int *corr_id)
{
	int ret;

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_START,
	};

	LOG_INF("Enqueing START command (correlation id: %d)", item.corr_id);

	if (corr_id) {
		*corr_id = item.corr_id;
	}

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_gnss_stop(bool keep_bckp_domain, int *corr_id)
{
	int ret;

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_STOP,
		.data.stop.keep_bckp_domain = keep_bckp_domain,
	};

	LOG_INF("Enqueing STOP command (correlation id: %d)", item.corr_id);

	if (corr_id) {
		*corr_id = item.corr_id;
	}

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_gnss_is_running(bool *running)
{
	if (!running) {
		return -EINVAL;
	}

	*running = m_running;

	return 0;
}
