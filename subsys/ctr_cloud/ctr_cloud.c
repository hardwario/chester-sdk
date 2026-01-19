/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_msg.h"
#include "ctr_cloud_packet.h"
#include "ctr_cloud_transfer.h"
#include "ctr_cloud_process.h"
#include "ctr_cloud_util.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_info.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/dfu/mcuboot.h>

#include <tinycrypt/constants.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVENT_INITIALIZED_SET BIT(0)
#define WORK_Q_STACK_SIZE     4096
#define WORK_Q_PRIORITY       K_LOWEST_APPLICATION_THREAD_PRIO

#define POLL_TIMEOUT     K_MINUTES(1)
#define POLL_RETRY_DELAY K_MINUTES(5)

LOG_MODULE_REGISTER(ctr_cloud, CONFIG_CTR_CLOUD_LOG_LEVEL);

static K_MUTEX_DEFINE(m_lock);
CTR_BUF_DEFINE_STATIC(m_transfer_buf, CTR_CLOUD_TRANSFER_BUF_SIZE);

static K_THREAD_STACK_DEFINE(m_work_q_stack, WORK_Q_STACK_SIZE);
static struct k_work_q m_work_q;

static K_EVENT_DEFINE(m_cloud_events);

static ctr_cloud_cb m_user_cb;
static void *m_user_data;
static struct ctr_cloud_options *m_options = NULL;
static struct ctr_cloud_session m_session;
static k_timeout_t m_poll_period = K_NO_WAIT;

static K_MUTEX_DEFINE(m_lock_state);

static struct ctr_cloud_metrics m_metrics = {
	.uplink_last_ts = -1,
	.uplink_error_last_ts = -1,
	.downlink_last_ts = -1,
	.downlink_error_last_ts = -1,
	.poll_last_ts = -1,
	.uplink_data_last_ts = -1,
	.downlink_data_last_ts = -1,
	.recv_shell_last_ts = -1,
};
static K_MUTEX_DEFINE(m_lock_metrics);

static void transfer_cb(enum ctr_cloud_transfer_event event,
			const struct ctr_cloud_transfer_event_data *data)
{
	k_mutex_lock(&m_lock_metrics, K_FOREVER);

	switch (event) {
	case CTR_CLOUD_TRANSFER_EVENT_UPLINK_OK:
		m_metrics.uplink_count++;
		m_metrics.uplink_bytes += data->bytes;
		m_metrics.uplink_fragments += data->fragments;
		ctr_rtc_get_ts(&m_metrics.uplink_last_ts);
		break;
	case CTR_CLOUD_TRANSFER_EVENT_UPLINK_ERROR:
		m_metrics.uplink_errors++;
		ctr_rtc_get_ts(&m_metrics.uplink_error_last_ts);
		break;
	case CTR_CLOUD_TRANSFER_EVENT_DOWNLINK_OK:
		m_metrics.downlink_count++;
		m_metrics.downlink_bytes += data->bytes;
		m_metrics.downlink_fragments += data->fragments;
		ctr_rtc_get_ts(&m_metrics.downlink_last_ts);
		break;
	case CTR_CLOUD_TRANSFER_EVENT_DOWNLINK_ERROR:
		m_metrics.downlink_errors++;
		ctr_rtc_get_ts(&m_metrics.downlink_error_last_ts);
		break;
	case CTR_CLOUD_TRANSFER_EVENT_POLL:
		m_metrics.poll_count++;
		ctr_rtc_get_ts(&m_metrics.poll_last_ts);
		break;
	}

	k_mutex_unlock(&m_lock_metrics);
}

static int process_downlink(struct ctr_buf *buf, struct ctr_buf *upbuf)
{
	int ret;

	size_t len = ctr_buf_get_used(buf);
	if (!len) {
		return 0; // No data to process
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	// LOG_HEXDUMP_INF(p, len, "process_downlink:");

	uint8_t type = p[0];

	switch (type) {
	case DL_SET_SESSION:
		LOG_DBG("Received session");

		k_mutex_lock(&m_lock_state, K_FOREVER);

		ret = ctr_cloud_msg_unpack_set_session(buf, &m_session);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_unpack_set_session` failed: %d", ret);
			k_mutex_unlock(&m_lock_state);
			return ret;
		}

		k_mutex_unlock(&m_lock_state);

		int64_t old_ts;
		ctr_rtc_get_ts(&old_ts);

		ret = ctr_rtc_set_ts(m_session.timestamp);
		if (ret) {
			LOG_WRN("Call `ctr_rtc_set_ts` failed: %d", ret);
		} else {
			k_mutex_lock(&m_lock_metrics, K_FOREVER);
			ctr_cloud_util_adjust_metrics_ts(&m_metrics, m_session.timestamp - old_ts);
			k_mutex_unlock(&m_lock_metrics);
		}

		LOG_INF("Session id %d", m_session.id);
		LOG_INF("Session decoder_hash %08llx", m_session.decoder_hash);
		LOG_INF("Session encoder_hash %08llx", m_session.encoder_hash);
		LOG_INF("Session config_hash %08llx", m_session.config_hash);
		LOG_INF("Session timestamp %lld", m_session.timestamp);
		LOG_INF("Session device_id: %s", m_session.device_id);
		LOG_INF("Session device_name: %s", m_session.device_name);

		break;
	case DL_SET_TIMESTAMP: {
		LOG_DBG("Received timestamp");
		uint64_t ts;
		ret = ctr_cloud_msg_unpack_set_timestamp(buf, &ts);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_unpack_set_timestamp` failed: %d", ret);
			return ret;
		}

		int64_t old_ts;
		ctr_rtc_get_ts(&old_ts);

		ret = ctr_rtc_set_ts(ts);
		if (ret) {
			LOG_WRN("Call `ctr_rtc_set_ts` failed: %d", ret);
		} else {
			k_mutex_lock(&m_lock_metrics, K_FOREVER);
			ctr_cloud_util_adjust_metrics_ts(&m_metrics, (int64_t)ts - old_ts);
			k_mutex_unlock(&m_lock_metrics);
		}
		break;
	}
	case DL_DOWNLOAD_CONFIG:
		LOG_DBG("Received config");

		struct ctr_cloud_msg_dlconfig config;
		ret = ctr_cloud_msg_unpack_config(buf, &config);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_unpack_config` failed: %d", ret);
			return ret;
		}
		ret = ctr_cloud_process_dlconfig(&config);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_process_dlconfig` failed: %d", ret);
			return ret;
		}
		break;
	case DL_DOWNLOAD_DATA:
		LOG_DBG("Received data");

		k_mutex_lock(&m_lock_metrics, K_FOREVER);
		m_metrics.downlink_data_count++;
		ctr_rtc_get_ts(&m_metrics.downlink_data_last_ts);
		k_mutex_unlock(&m_lock_metrics);

		if (m_user_cb) {
			union ctr_cloud_event_data data = {
				.recv.buf = &p[1],
				.recv.len = len - 1,
			};

			m_user_cb(CTR_CLOUD_EVENT_RECV, &data, m_user_data);
		} else {
			LOG_WRN("No user callback set");
		}
		break;
	case DL_DOWNLOAD_SHELL:
		LOG_DBG("Received shell");

		k_mutex_lock(&m_lock_metrics, K_FOREVER);
		m_metrics.recv_shell_count++;
		ctr_rtc_get_ts(&m_metrics.recv_shell_last_ts);
		k_mutex_unlock(&m_lock_metrics);

		struct ctr_cloud_msg_dlshell msg;
		ret = ctr_cloud_msg_unpack_dlshell(buf, &msg);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_unpack_dlshell` failed: %d", ret);
			return ret;
		}
		ret = ctr_cloud_process_dlshell(&msg, upbuf);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_process_dlshell` failed: %d", ret);
			return ret;
		}
		break;
	case DL_DOWNLOAD_FIRMWARE:
		LOG_DBG("Received firmware");
		struct ctr_cloud_msg_dlfirmware firmware;
		ret = ctr_cloud_msg_unpack_dlfirmware(buf, &firmware);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_unpack_dlfirmware` failed: %d", ret);
			return ret;
		}
		ret = ctr_cloud_process_dlfirmware(&firmware, upbuf);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_process_dlfirmware` failed: %d", ret);
			return ret;
		}
		break;
	default:
		LOG_ERR("Unknown downlink type: %d", type);
		break;
	}

	return 0;
}

static void poll_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(m_poll_dwork, poll_work_handler);

static void poll_work_handler(struct k_work *work)
{
	int ret = ctr_cloud_downlink(POLL_TIMEOUT);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_downlink` failed: %d, retry scheduled", ret);
		k_work_schedule_for_queue(&m_work_q, &m_poll_dwork, POLL_RETRY_DELAY);
	}
}

static void poll_timer_handler(struct k_timer *timer)
{
	k_work_schedule_for_queue(&m_work_q, &m_poll_dwork, K_NO_WAIT);
}

static K_TIMER_DEFINE(m_poll_timer, poll_timer_handler, NULL);

static int uplink(struct ctr_buf *buf, k_timeout_t timeout)
{
	int ret;
	bool has_downlink = false;

	ret = ctr_cloud_transfer_uplink(buf, &has_downlink, timeout);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_transfer_uplink` failed: %d", ret);
		return ret;
	}

	if (has_downlink) {
		k_work_schedule_for_queue(&m_work_q, &m_poll_dwork, K_NO_WAIT);
	}

	return 0;
}

#define ASERT_SESSION_AND_OPTIONS(pmutex)                                                          \
	if (m_session.id == 0) {                                                                   \
		LOG_WRN("Session ID is not set");                                                  \
		k_mutex_unlock(pmutex);                                                            \
		return -EPERM;                                                                     \
	}                                                                                          \
	if (m_options == NULL) {                                                                   \
		LOG_WRN("Options is not set");                                                     \
		k_mutex_unlock(pmutex);                                                            \
		return -EPERM;                                                                     \
	}

static int create_session(void)
{
	int ret;
	bool has_downlink = false;

	k_mutex_lock(&m_lock, K_FOREVER);

	ctr_buf_reset(&m_transfer_buf);

	ret = ctr_cloud_msg_pack_create_session(&m_transfer_buf);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_pack_create_session` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = ctr_cloud_transfer_uplink(&m_transfer_buf, &has_downlink, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_transfer_uplink` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	if (!has_downlink) {
		LOG_ERR("No session response from server");
		k_mutex_unlock(&m_lock);
		return -EIO;
	}

	ctr_buf_reset(&m_transfer_buf);

	ret = ctr_cloud_transfer_downlink(&m_transfer_buf, &has_downlink, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_transfer_downlink` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = process_downlink(&m_transfer_buf, NULL);
	if (ret) {
		LOG_ERR("Call `process_downlink` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	k_mutex_unlock(&m_lock);

	if (has_downlink) {
		k_work_schedule_for_queue(&m_work_q, &m_poll_dwork, K_NO_WAIT);
	}

	return 0;
}

static int upload_decoder(void)
{
	int ret;

	k_mutex_lock(&m_lock_state, K_FOREVER);
	if (m_session.id == 0) {
		LOG_ERR("Session ID is not set");
		k_mutex_unlock(&m_lock_state);
		return -EPERM;
	}
	k_mutex_unlock(&m_lock_state);

	k_mutex_lock(&m_lock, K_FOREVER);

	if (m_options->decoder_buf == NULL || m_options->decoder_len == 0) {
		LOG_WRN("Decoder is not set");
		k_mutex_unlock(&m_lock);
		return 0;
	}

	if (m_session.decoder_hash != m_options->decoder_hash) {
		LOG_INF("Uploading decoder hash: %08llx", m_options->decoder_hash);

		ctr_buf_reset(&m_transfer_buf);

		ret = ctr_cloud_msg_pack_decoder(&m_transfer_buf, m_options->decoder_hash,
						 m_options->decoder_buf, m_options->decoder_len);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_pack_decoder` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		ret = uplink(&m_transfer_buf, K_FOREVER);
		if (ret) {
			LOG_ERR("Call `transfer` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		m_session.decoder_hash = m_options->decoder_hash;

		LOG_INF("Uploading decoder finished");
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

static int upload_encoder(void)
{
	int ret;

	k_mutex_lock(&m_lock_state, K_FOREVER);
	if (m_session.id == 0) {
		LOG_ERR("Session ID is not set");
		k_mutex_unlock(&m_lock_state);
		return -EPERM;
	}
	k_mutex_unlock(&m_lock_state);

	k_mutex_lock(&m_lock, K_FOREVER);

	if (m_options->encoder_buf == NULL || m_options->encoder_len == 0) {
		LOG_INF("Encoder is not set");
		k_mutex_unlock(&m_lock);
		return 0;
	}

	if (m_session.encoder_hash != m_options->encoder_hash) {
		LOG_INF("Uploading encoder hash: %08llx", m_options->encoder_hash);

		ctr_buf_reset(&m_transfer_buf);

		ret = ctr_cloud_msg_pack_encoder(&m_transfer_buf, m_options->encoder_hash,
						 m_options->encoder_buf, m_options->encoder_len);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_msg_pack_encoder` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		ret = uplink(&m_transfer_buf, K_FOREVER);
		if (ret) {
			LOG_ERR("Call `transfer` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		m_session.encoder_hash = m_options->encoder_hash;

		LOG_INF("Uploading encoder finished");
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

static int upload_config(void)
{
	int ret;

	k_mutex_lock(&m_lock_state, K_FOREVER);
	if (m_session.id == 0) {
		LOG_ERR("Session ID is not set");
		k_mutex_unlock(&m_lock_state);
		return -EPERM;
	}
	k_mutex_unlock(&m_lock_state);

	k_mutex_lock(&m_lock, K_FOREVER);

	ctr_buf_reset(&m_transfer_buf);

	ret = ctr_cloud_msg_pack_config(&m_transfer_buf);

	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_pack_config` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	uint64_t hash;

	ret = ctr_cloud_msg_get_hash(&m_transfer_buf, &hash);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_get_hash` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	if (m_session.config_hash != hash) {
		LOG_INF("Uploading config hash: %08llx", hash);

		ret = uplink(&m_transfer_buf, K_FOREVER);
		if (ret) {
			LOG_ERR("Call `transfer` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		m_session.config_hash = hash;

		LOG_INF("Uploading config finished");
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
static int firmware_confirmed(void)
{
	int ret;

	ctr_cloud_uuid_t fuid;

	ret = ctr_cloud_util_get_firmware_update_id(fuid);

	if (ret) {
		return 0; // No firmware update ID
	}

	ctr_cloud_util_delete_firmware_update_id();

	if (boot_is_img_confirmed()) {
		LOG_INF("Image confirmed OK");
		return 0;
	} else {
		if (boot_write_img_confirmed()) {
			LOG_ERR("Failed to confirm image");
			return -EIO; // Failed to confirm image
		} else {
			LOG_INF("Marked image as OK");
		}
	}

	struct ctr_cloud_upfirmware upfirmware = {
		.target = "app",
		.type = "ack",
		.id = fuid,
	};

	k_mutex_lock(&m_lock, K_FOREVER);

	ctr_buf_reset(&m_transfer_buf);

	ret = ctr_cloud_msg_pack_firmware(&m_transfer_buf, &upfirmware);
	if (ret) {
		LOG_ERR("ctr_cloud_msg_pack_firmware failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = uplink(&m_transfer_buf, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `transfer` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	k_mutex_unlock(&m_lock);

	return 0;
}
#endif

static void init_work_handler(struct k_work *work)
{
	int ret;

	LOG_INF("Initializing Start");

	k_mutex_lock(&m_lock, K_FOREVER);

	ctr_buf_reset(&m_transfer_buf);

	for (;;) {

		ctr_cloud_transfer_wait_for_ready(K_FOREVER);

		LOG_INF("Running CREATE SESSION");
		ret = create_session();
		if (ret) {
			LOG_ERR("Call `create_session` failed: %d", ret);
			continue;
		}

		LOG_INF("Running UPLOAD ENCODER");

		ret = upload_decoder();
		if (ret) {
			LOG_ERR("Call `upload_decoder` failed: %d", ret);
			continue;
		}

		LOG_INF("Running UPLOAD ENCODER");

		ret = upload_encoder();

		if (ret) {
			LOG_ERR("Call `upload_encoder` failed: %d", ret);
			continue;
		}

		LOG_INF("Running UPLOAD CONFIG");

		ret = upload_config();

		if (ret) {
			LOG_WRN("Call `upload_config` failed: %d", ret);
		}

#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
		firmware_confirmed();
#endif

		break;
	}

	k_mutex_unlock(&m_lock);

	k_event_post(&m_cloud_events, EVENT_INITIALIZED_SET);

	if (!K_TIMEOUT_EQ(m_poll_period, K_NO_WAIT)) {
		k_timer_start(&m_poll_timer, m_poll_period, m_poll_period);
	}
}

static K_WORK_DEFINE(m_init_work, init_work_handler);

int ctr_cloud_init(struct ctr_cloud_options *options)
{
	int ret;

	if (!options) {
		LOG_ERR("Invalid argument");
		return -EINVAL;
	}

	m_options = options;

	uint32_t serial_number;

	ret = ctr_info_get_serial_number_uint32(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
		return ret;
	}

	char *claim_token_str;
	ret = ctr_info_get_claim_token(&claim_token_str);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_claim_token` failed: %d", ret);
		return ret;
	}

	if (!strcmp(claim_token_str, "(unset)")) {
		LOG_ERR("Claim token is not set");
		return -EPERM;
	}

	uint8_t token[16];

	size_t size = hex2bin(claim_token_str, strlen(claim_token_str), token, sizeof(token));
	if (size != sizeof(token)) {
		LOG_ERR("Call `hex2bin` failed");
		return -EINVAL;
	}

	ret = ctr_cloud_transfer_init(serial_number, token, transfer_cb);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_transfer_init` failed: %d", ret);
		return ret;
	}

	k_mutex_init(&m_lock);
	k_mutex_init(&m_lock_state);

	k_work_queue_init(&m_work_q);
	k_work_queue_start(&m_work_q, m_work_q_stack, K_THREAD_STACK_SIZEOF(m_work_q_stack),
			   WORK_Q_PRIORITY, NULL);

	k_work_submit_to_queue(&m_work_q, &m_init_work);

	return 0;
}

int ctr_cloud_wait_initialized(k_timeout_t timeout)
{
	if (k_event_wait(&m_cloud_events, EVENT_INITIALIZED_SET, false, K_NO_WAIT)) {
		return 0;
	}

	LOG_DBG("Waiting for initialized to be set");

	if (k_event_wait(&m_cloud_events, EVENT_INITIALIZED_SET, false, timeout)) {
		LOG_DBG("Initialized has been set - continuing");
		return 0;
	}

	return -ETIMEDOUT;
}

int ctr_cloud_is_initialized(bool *initialized)
{
	if (!initialized) {
		return -EINVAL;
	}
	*initialized = k_event_test(&m_cloud_events, EVENT_INITIALIZED_SET) != 0;
	return 0;
}

int ctr_cloud_set_callback(ctr_cloud_cb user_cb, void *user_data)
{
	k_mutex_lock(&m_lock, K_FOREVER);

	m_user_cb = user_cb;
	m_user_data = user_data;

	k_mutex_unlock(&m_lock);

	return 0;
}

// int ctr_cloud_get_timestamp(void)
// {
// 	int ret;

// 	k_mutex_lock(&m_lock, K_FOREVER);

// 	LOG_INF("Request GET TIMESTAMP");

// 	ctr_buf_reset(&m_transfer_buf);

// 	ret = ctr_cloud_msg_pack_get_timestamp(&m_transfer_buf);
// 	if (ret) {
// 		LOG_ERR("Call `ctr_cloud_msg_pack_get_timestamp` failed: %d", ret);
// 		k_mutex_unlock(&m_lock);
// 		return ret;
// 	}

// 	ret = uplink(&m_transfer_buf, K_FOREVER);
// 	if (ret) {
// 		LOG_ERR("Call `transfer` failed: %d", ret);
// 		k_mutex_unlock(&m_lock);
// 		return ret;
// 	}

// 	k_mutex_unlock(&m_lock);

// 	return 0;
// }

int ctr_cloud_set_poll_interval(k_timeout_t interval)
{
	m_poll_period = interval;

	if ((m_cloud_events.events & EVENT_INITIALIZED_SET) == 0) {
		return 0;
	}

	if (K_TIMEOUT_EQ(m_poll_period, K_NO_WAIT)) {
		k_timer_stop(&m_poll_timer);
	} else {
		k_timer_start(&m_poll_timer, m_poll_period, m_poll_period);
	}

	return 0;
}

int ctr_cloud_poll_immediately(void)
{
	k_work_schedule_for_queue(&m_work_q, &m_poll_dwork, K_NO_WAIT);

	return 0;
}

int ctr_cloud_send_data(const void *buf, size_t len, k_timeout_t timeout)
{
	int ret;

	if (!buf || !len) {
		return -EINVAL;
	}

	k_timepoint_t end = sys_timepoint_calc(timeout);

	ret = k_mutex_lock(&m_lock, timeout);
	if (ret) {
		LOG_WRN("Failed to acquire lock: %d", ret);
		return ret;
	}

	LOG_INF("Request SEND started");

	ctr_buf_reset(&m_transfer_buf);

	ret = ctr_buf_append_u8(&m_transfer_buf, UL_UPLOAD_DATA);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = ctr_buf_append_u64_be(&m_transfer_buf, m_options->decoder_hash);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u64` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = ctr_buf_append_mem(&m_transfer_buf, buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = uplink(&m_transfer_buf, sys_timepoint_timeout(end));
	if (ret) {
		LOG_ERR("Call `transfer` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	k_mutex_lock(&m_lock_metrics, K_FOREVER);
	m_metrics.uplink_data_count++;
	ctr_rtc_get_ts(&m_metrics.uplink_data_last_ts);
	k_mutex_unlock(&m_lock_metrics);

	k_mutex_unlock(&m_lock);

	LOG_INF("Request SEND finished");

	return 0;
}

int ctr_cloud_send(const void *buf, size_t len)
{
	return ctr_cloud_send_data(buf, len, K_FOREVER);
}

int ctr_cloud_downlink(k_timeout_t timeout)
{
	int ret;
	bool has_downlink = false;
	k_timepoint_t end = sys_timepoint_calc(timeout);

	ret = k_mutex_lock(&m_lock, timeout);
	if (ret) {
		LOG_WRN("Failed to acquire lock: %d", ret);
		return ret;
	}

	LOG_DBG("Request RECV started");

	ctr_buf_reset(&m_transfer_buf);

	ret = ctr_cloud_transfer_downlink(&m_transfer_buf, &has_downlink,
					  sys_timepoint_timeout(end));
	if (ret) {
		LOG_WRN("Call `ctr_cloud_transfer_downlink` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	struct ctr_buf upbuf = {
		.mem = ctr_buf_get_mem(&m_transfer_buf) + ctr_buf_get_used(&m_transfer_buf),
		.size = ctr_buf_get_free(&m_transfer_buf),
		.len = 0,
	};

	ret = process_downlink(&m_transfer_buf, &upbuf);
	if (ret) {
		LOG_WRN("Call `process_downlink` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	if (ctr_buf_get_used(&upbuf) > 0) {
		ret = ctr_cloud_transfer_uplink(&upbuf, &has_downlink, K_FOREVER);
		if (ret) {
			LOG_WRN("Call `ctr_cloud_transfer_uplink` for response failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}
	}

	k_mutex_unlock(&m_lock);

	if (has_downlink) {
		k_work_schedule_for_queue(&m_work_q, &m_poll_dwork, K_NO_WAIT);
	}

	LOG_DBG("Request RECV finished");

	return 0;
}

int ctr_cloud_get_last_seen_ts(int64_t *ts)
{
	if (!ts) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock_state, K_FOREVER);

	ASERT_SESSION_AND_OPTIONS(&m_lock_state)

	k_mutex_unlock(&m_lock_state);

	k_mutex_lock(&m_lock_metrics, K_FOREVER);

	int64_t last_seen = 0;
	if (m_metrics.uplink_last_ts > last_seen) {
		last_seen = m_metrics.uplink_last_ts;
	}
	if (m_metrics.downlink_last_ts > last_seen) {
		last_seen = m_metrics.downlink_last_ts;
	}
	if (m_metrics.poll_last_ts > last_seen) {
		last_seen = m_metrics.poll_last_ts;
	}

	k_mutex_unlock(&m_lock_metrics);

	*ts = last_seen;

	return 0;
}

int ctr_cloud_get_metrics(struct ctr_cloud_metrics *metrics)
{
	if (!metrics) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock_metrics, K_FOREVER);
	memcpy(metrics, &m_metrics, sizeof(m_metrics));
	ctr_rtc_get_ts(&metrics->timestamp);
	k_mutex_unlock(&m_lock_metrics);

	return 0;
}

int ctr_cloud_firmware_update(const char *firmware)
{
	int ret;

	size_t len = strlen(firmware);
	if (len == 0) {
		LOG_ERR("Invalid argument `firmware` is empty");
		return -EINVAL;
	}

	if (len > 200) {
		LOG_ERR("Invalid argument `firmware` is too long");
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	LOG_INF("Request FIRMWARE UPDATE started");

	ctr_buf_reset(&m_transfer_buf);

	struct ctr_cloud_upfirmware upfirmware = {
		.target = "app",
		.type = "download",
		.max_length = 256,
		.firmware = firmware,
	};

	ret = ctr_cloud_msg_pack_firmware(&m_transfer_buf, &upfirmware);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_pack_firmware` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = uplink(&m_transfer_buf, K_FOREVER);

	if (ret) {
		LOG_ERR("Call `transfer` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	k_mutex_unlock(&m_lock);

	return 0;
}
