/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_packet.h"
#include "ctr_cloud_transfer.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lte_v2.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODEM_SLEEP_DELAY K_SECONDS(10)

LOG_MODULE_REGISTER(ctr_cloud_transfer, CONFIG_CTR_CLOUD_LOG_LEVEL);

CTR_BUF_DEFINE_STATIC(m_send_buf, CTR_CLOUD_PACKET_MAX_SIZE);
CTR_BUF_DEFINE_STATIC(m_recv_buf, CTR_CLOUD_PACKET_MAX_SIZE);

static uint16_t m_sequence;
static uint16_t m_last_recv_sequence;

static uint8_t m_token[16];
static struct ctr_cloud_packet m_pck_send;
static struct ctr_cloud_packet m_pck_recv;
static struct ctr_cloud_transfer_metrics m_metrics = {
	.uplink_last_ts = -1,
	.downlink_last_ts = -1,
};
static K_MUTEX_DEFINE(m_lock_metrics);

static int send_recv(struct ctr_buf *send_buf, struct ctr_buf *recv_buf, bool rai)
{
	int ret;

	LOG_HEXDUMP_INF(ctr_buf_get_mem(&m_send_buf), ctr_buf_get_used(&m_send_buf),
			rai ? "Sending packet RAI:" : "Sending packet:");

	size_t recv_len;

	struct ctr_lte_v2_send_recv_param param = {
		.rai = rai,
		.send_buf = ctr_buf_get_mem(send_buf),
		.send_len = ctr_buf_get_used(send_buf),
		.recv_buf = NULL,
		.recv_size = 0,
		.recv_len = &recv_len,
		.timeout = K_FOREVER,
	};

	if (recv_buf) {
		ctr_buf_reset(recv_buf);
		param.recv_buf = ctr_buf_get_mem(recv_buf);
		param.recv_size = ctr_buf_get_free(recv_buf);
	}

	ret = ctr_lte_v2_send_recv(&param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_send_recv` failed: %d", ret);
		return ret;
	}

	if (recv_buf) {
		ret = ctr_buf_seek(&m_recv_buf, recv_len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
			return ret;
		}

		if (!ctr_buf_get_used(&m_recv_buf)) {
			LOG_ERR("No data received");
			return -EIO;
		}

		// LOG_HEXDUMP_INF(ctr_buf_get_mem(&m_recv_buf), ctr_buf_get_used(&m_recv_buf),
		// 		"Received packet:");
	}

	return 0;
}

int ctr_cloud_transfer_init(uint32_t serial_number, uint8_t token[16])
{
	memset(&m_pck_send, 0, sizeof(m_pck_send));
	memset(&m_pck_recv, 0, sizeof(m_pck_recv));

	m_pck_send.serial_number = serial_number;
	memcpy(m_token, token, sizeof(m_token));

	m_sequence = 0;
	m_last_recv_sequence = 0;

	ctr_cloud_transfer_reset_metrics();

	ctr_lte_v2_enable();

	return 0;
}

int ctr_cloud_transfer_wait_for_ready(k_timeout_t timeout)
{
	int ret;

	ret = ctr_lte_v2_wait_for_connected(timeout);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_wait_for_connected` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_transfer_reset_metrics(void)
{
	k_mutex_lock(&m_lock_metrics, K_FOREVER);
	memset(&m_metrics, 0, sizeof(m_metrics));
	k_mutex_unlock(&m_lock_metrics);
	return 0;
}

int ctr_cloud_transfer_get_metrics(struct ctr_cloud_transfer_metrics *metrics)
{
	if (!metrics) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock_metrics, K_FOREVER);
	memcpy(metrics, &m_metrics, sizeof(m_metrics));
	k_mutex_unlock(&m_lock_metrics);

	return 0;
}

int ctr_cloud_transfer_uplink(struct ctr_buf *buf, bool *has_downlink)
{
	int ret = 0;
	int res = 0;
	uint8_t *p = NULL;
	size_t len = 0;
	int part = 0;
	int fragments = 0;

	if (has_downlink != NULL) {
		*has_downlink = false;
	}

restart:
	part = 0;

	if (buf) {
		p = ctr_buf_get_mem(buf);
		len = ctr_buf_get_used(buf);

		fragments = len / CTR_CLOUD_DATA_MAX_SIZE;
		if (len % CTR_CLOUD_DATA_MAX_SIZE) {
			fragments++;
		}
	}

	do {
		LOG_INF("Processing part: %d (%d left)", part, fragments - part - 1);

		ctr_buf_reset(&m_recv_buf);

		m_pck_send.data_len = 0;
		m_pck_send.flags = 0;
		m_pck_send.data = NULL;

		if (buf) {
			m_pck_send.data = p;
			m_pck_send.data_len = MIN(len, CTR_CLOUD_DATA_MAX_SIZE);

			p += m_pck_send.data_len;
			len -= m_pck_send.data_len;

			if (part == 0) {
				m_pck_send.flags |= CTR_CLOUD_PACKET_FLAG_FIRST;
			}
			if (len == 0) {
				m_pck_send.flags |= CTR_CLOUD_PACKET_FLAG_LAST;
			}

			part++;
		}

		m_pck_send.sequence = m_sequence;
		m_sequence = ctr_cloud_packet_sequence_inc(m_sequence);

	again:
		LOG_INF("Sending packet Sequence: %u %s len: %u", m_pck_send.sequence,
			ctr_cloud_packet_flags_to_str(m_pck_send.flags), m_pck_send.data_len);

		ret = ctr_cloud_packet_pack(&m_pck_send, m_token, &m_send_buf);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_packet_pack` failed: %d", ret);
			res = ret;
			goto exit;
		}

		ctr_buf_reset(&m_recv_buf);
		bool rai = m_pck_send.flags & CTR_CLOUD_PACKET_FLAG_LAST;
		ret = send_recv(&m_send_buf, &m_recv_buf, rai);
		if (ret) {
			LOG_ERR("Call `send_recv` failed: %d", ret);
			res = ret;
			goto exit;
		}

		ret = ctr_cloud_packet_unpack(&m_pck_recv, m_token, &m_recv_buf);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_packet_unpack` failed: %d", ret);
			res = ret;
			goto exit;
		}

		LOG_INF("Received packet Sequence: %u %s len: %u", m_pck_recv.sequence,
			ctr_cloud_packet_flags_to_str(m_pck_recv.flags), m_pck_recv.data_len);

		if (m_pck_recv.serial_number != m_pck_send.serial_number) {
			LOG_ERR("Serial number mismatch");
			res = -EREMCHG;
			goto exit;
		}

		if (has_downlink != NULL) {
			*has_downlink = m_pck_recv.flags & CTR_CLOUD_PACKET_FLAG_POLL;
		}

		if (m_pck_recv.flags & (CTR_CLOUD_PACKET_FLAG_FIRST | CTR_CLOUD_PACKET_FLAG_LAST)) {
			LOG_ERR("Received unexpected flags");
			res = -EIO;
			goto exit;
		}

		if (m_pck_recv.sequence == 0) {
			LOG_WRN("Received sequence reset request");
			m_sequence = 0;
			goto restart;
		}

		if (m_pck_recv.sequence != m_sequence) {
			if (m_pck_recv.sequence == m_last_recv_sequence) {
				LOG_WRN("Received repeat response");
				goto again;
			} else {
				LOG_WRN("Received unexpected sequence expect: %u", m_sequence);
				m_sequence = 0;
				goto restart;
			}
		}

		m_last_recv_sequence = m_pck_recv.sequence;
		m_sequence = ctr_cloud_packet_sequence_inc(m_sequence);

		if (m_pck_recv.data_len != 0) {
			LOG_ERR("Received unexpected data length");
			res = -EIO;
			goto exit;
		}

	} while (len);

exit:
	if (res) {
		LOG_ERR("Transfer uplink failed: %d reset sequence", res);
		m_sequence = 0;
		m_last_recv_sequence = 0;
	}

	k_mutex_lock(&m_lock_metrics, K_FOREVER);
	if (res) {
		m_metrics.uplink_errors++;
	} else {
		m_metrics.uplink_count++;
		m_metrics.uplink_bytes += ctr_buf_get_used(buf);
		m_metrics.uplink_fragments += fragments;
		ret = ctr_rtc_get_ts(&m_metrics.uplink_last_ts);

		if (ret) {
			LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
		}
	}
	k_mutex_unlock(&m_lock_metrics);

	return res;
}

int ctr_cloud_transfer_downlink(struct ctr_buf *buf, bool *has_downlink)
{
	int ret;
	int res = 0;
	int part = 0;
	bool quit;

	*has_downlink = false;

	size_t buf_used = ctr_buf_get_used(buf);

restart:

	part = 0;
	quit = false;

	do {
		LOG_INF("Processing part: %d", part);

		m_pck_send.flags =
			part == 0 ? CTR_CLOUD_PACKET_FLAG_POLL : CTR_CLOUD_PACKET_FLAG_ACK;
		m_pck_send.data = NULL;
		m_pck_send.data_len = 0;
		m_pck_send.sequence = m_sequence;
		m_sequence = ctr_cloud_packet_sequence_inc(m_sequence);

	again:
		LOG_INF("Sending packet Sequence: %u %s len: %u", m_pck_send.sequence,
			ctr_cloud_packet_flags_to_str(m_pck_send.flags), m_pck_send.data_len);

		ret = ctr_cloud_packet_pack(&m_pck_send, m_token, &m_send_buf);

		if (ret) {
			LOG_ERR("Call `ctr_cloud_packet_pack` failed: %d", ret);
			res = ret;
			goto exit;
		}

		ctr_buf_reset(&m_recv_buf);
		if (quit) {
			bool rai = !*has_downlink; // use RAI if no downlink is expected
			ret = send_recv(&m_send_buf, NULL, rai);
			if (ret) {
				LOG_ERR("Call `send_recv` failed: %d", ret);
				res = ret;
				goto exit;
			}
			break;
		}

		bool rai = part == 0;

		ret = send_recv(&m_send_buf, &m_recv_buf, rai);
		if (ret) {
			LOG_ERR("Call `send_recv` failed: %d", ret);
			res = ret;
			goto exit;
		}

		ret = ctr_cloud_packet_unpack(&m_pck_recv, m_token, &m_recv_buf);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_packet_unpack` failed: %d", ret);
			res = ret;
			goto exit;
		}

		LOG_INF("Received packet Sequence: %u %s len: %u", m_pck_recv.sequence,
			ctr_cloud_packet_flags_to_str(m_pck_recv.flags), m_pck_recv.data_len);

		if (m_pck_recv.serial_number != m_pck_send.serial_number) {
			LOG_ERR("Serial number mismatch");
			res = -EREMCHG;
			goto exit;
		}

		if (m_pck_recv.sequence == 0) {
			LOG_WRN("Received sequence reset request");
			m_sequence = 0;
			goto restart;
		}

		if (m_pck_recv.sequence != m_sequence) {
			if (m_pck_recv.sequence == m_last_recv_sequence) {
				LOG_WRN("Received repeat response");
				goto again;
			} else {
				LOG_WRN("Received unexpected sequence expect: %u", m_sequence);
				m_sequence = 0;
				goto restart;
			}
		}

		m_last_recv_sequence = m_pck_recv.sequence;
		m_sequence = ctr_cloud_packet_sequence_inc(m_sequence);

		if (m_pck_recv.flags & CTR_CLOUD_PACKET_FLAG_ACK) {
			LOG_ERR("Received unexpected flags");
			res = -EIO;
			goto exit;
		}

		if (m_pck_recv.flags & CTR_CLOUD_PACKET_FLAG_FIRST) {
			ctr_buf_reset(buf);
		}

		ret = ctr_buf_append_mem(buf, m_pck_recv.data, m_pck_recv.data_len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
			res = ret;
			goto exit;
		}

		quit = m_pck_recv.flags & CTR_CLOUD_PACKET_FLAG_LAST;
		*has_downlink = m_pck_recv.flags & CTR_CLOUD_PACKET_FLAG_POLL;

		if (quit && m_pck_recv.data_len == 0 && part == 0) {
			LOG_INF("Skip ack response");
			break;
		}

		part++;

	} while (true);

exit:

	if (res) {
		LOG_ERR("Transfer downlink failed: %d reset sequence", res);
		m_sequence = 0;
		m_last_recv_sequence = 0;
	}

	k_mutex_lock(&m_lock_metrics, K_FOREVER);
	if (res) {
		m_metrics.downlink_errors++;
	} else {
		if (part) {
			m_metrics.downlink_count++;
			m_metrics.downlink_fragments += part;
			m_metrics.downlink_bytes += ctr_buf_get_used(buf) - buf_used;
			ret = ctr_rtc_get_ts(&m_metrics.downlink_last_ts);
			if (ret) {
				LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
			}
		} else {
			m_metrics.poll_count++;
			ret = ctr_rtc_get_ts(&m_metrics.poll_last_ts);
			if (ret) {
				LOG_WRN("Call `ctr_rtc_get_ts` failed: %d", ret);
			}
		}
	}
	k_mutex_unlock(&m_lock_metrics);

	return res;
}
