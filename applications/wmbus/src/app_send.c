/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */
#include "feature.h"
#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_send.h"
#include "packet.h"

/* CHESTER includes */
#include <chester/ctr_cloud.h>
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(FEATURE_SUBSYSTEM_LTE_V2)
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>
#include <zcbor_decode.h>
#endif /* defined(FEATURE_SUBSYSTEM_LTE_V2) */

/* Standard includes */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

#define POLL_PERIOD K_SECONDS(5)

#define WORK_Q_STACK_SIZE 4096
#define WORK_Q_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);
static struct k_work_q m_work_q;

__unused static void ctr_cloud_event_handler(enum ctr_cloud_event event,
					     union ctr_cloud_event_data *data, void *param)
{
	/*int ret;*/

	if (event == CTR_CLOUD_EVENT_RECV) {
		LOG_HEXDUMP_INF(data->recv.buf, data->recv.len, "Received:");

		if (data->recv.len < 8) {
			LOG_ERR("Missing encoder hash");
			return;
		}

		uint8_t *buf = (uint8_t *)data->recv.buf + 8;
		size_t len = data->recv.len - 8;

		ZCBOR_STATE_D(zs, 1, buf, len, 1, 0);

		/*ret = app_cbor_decode(zs);
		if (ret) {
			LOG_ERR("Call `app_cbor_decode` failed: %d", ret);
			return;
		}*/
	}
}

static void poll_work_handler(struct k_work *work)
{
#if defined(CONFIG_SHIELD_CTR_LTE_V2)
	int ret;

	LOG_WRN("Running RECV");

	ret = ctr_cloud_recv();
	if (ret) {
		LOG_ERR("Call `ctr_cloud_recv` failed: %d", ret);
		return;
	}

	LOG_WRN("Stopped RECV");

#endif /* defined(CONFIG_SHIELD_CTR_LTE_V2) */
}

static K_WORK_DEFINE(m_poll_work, poll_work_handler);

static void poll_timer_handler(struct k_timer *timer)
{
	k_work_submit_to_queue(&m_work_q, &m_poll_work);
}

static K_TIMER_DEFINE(m_poll_timer, poll_timer_handler, NULL);

static void upload_data_work_handler(struct k_work *work)
{

	LOG_WRN("Running UPLOAD DATA");

	atomic_set(&g_app_data.send_flag, true);

#if defined(CONFIG_SHIELD_CTR_LTE_V2)
	int ret;

	CTR_BUF_DEFINE_STATIC(buf, 4096);

	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		/* repeat sending as long as we have wM-BUS packets in receive buffer */
		while (1) {

			ctr_buf_reset(&buf);

			ZCBOR_STATE_E(zs, 8, ctr_buf_get_mem(&buf), ctr_buf_get_free(&buf), 1);

			ret = app_cbor_encode(zs, ctr_buf_get_mem(&buf));
			if (ret) {
				LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
				break;
			}

			size_t len = zs[0].payload_mut - ctr_buf_get_mem(&buf);

			ret = ctr_buf_seek(&buf, len);
			if (ret) {
				LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
				break;
			}

			LOG_INF("Calling `ctr_cloud_send`");
			ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
			if (ret) {
				LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
				break;
			}

			/* Is there more data to send? */
			size_t wmbus_packet_len;
			packet_get_next_size(&wmbus_packet_len);
			if (wmbus_packet_len) {
				atomic_inc(&g_app_data.send_index);
				LOG_INF("More data to be send ==============================");
			} else {
				LOG_INF("No more data to be send +++++++++++++++++++++++++++");
				break;
			}
		}
	}
#endif /* defined(CONFIG_SHIELD_CTR_LTE_V2) */

	/* All data sent */
	atomic_set(&g_app_data.working_flag, false);

	LOG_WRN("Stopped UPLOAD DATA");

	/* Store data if in enroll mode */
	if (g_app_data.enroll_mode) {
		app_config_enroll_save();
	}

	// Cleanup if no addresses configured, clean temporary list
	if (g_app_data.scan_all) {
		app_config_clear_address();
	}
}

static K_WORK_DEFINE(m_upload_data_work, upload_data_work_handler);

static void clear_work_handler(struct k_work *work)
{

	/* TODO clear wmbus payload and flags */
	packet_clear();
	atomic_set(&g_app_data.working_flag, 0);
}

static K_WORK_DEFINE(m_clear_work, clear_work_handler);

int app_send(void)
{
	k_work_submit_to_queue(&m_work_q, &m_upload_data_work);
	k_work_submit_to_queue(&m_work_q, &m_clear_work);

	return 0;
}

static int init()
{
	LOG_INF("System initialization");

	k_work_queue_init(&m_work_q);
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

#if defined(CONFIG_SHIELD_CTR_LTE_V2)

	int ret;
	ret = ctr_cloud_set_callback(ctr_cloud_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_set_callback` failed: %d", ret);
	}
#endif /* defined(CONFIG_SHIELD_CTR_LTE_V2) */

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int cmd_poll(const struct shell *shell, size_t argc, char **argv)
{
	k_work_submit_to_queue(&m_work_q, &m_poll_work);

	return 0;
}

SHELL_CMD_REGISTER(poll, NULL, "Poll cloud for new data immediately.", cmd_poll);
