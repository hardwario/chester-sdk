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

#include <zephyr/sys/base64.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODEM_SLEEP_DELAY K_SECONDS(10)

#define MAX_DATA_SIZE        (CTR_CLOUD_PACKET_MAX_SIZE - CTR_CLOUD_PACKET_MIN_SIZE)
#define MAX_BASE64_DATA_SIZE ((CTR_CLOUD_PACKET_MAX_SIZE / 4 * 3) - 7 - CTR_CLOUD_PACKET_MIN_SIZE)

LOG_MODULE_REGISTER(ctr_cloud_transfer, CONFIG_CTR_CLOUD_LOG_LEVEL);

CTR_BUF_DEFINE_STATIC(m_buf_0, CTR_CLOUD_PACKET_MAX_SIZE);
CTR_BUF_DEFINE_STATIC(m_buf_1, CTR_CLOUD_PACKET_MAX_SIZE);

static uint16_t m_sequence;
static uint16_t m_last_recv_sequence;

static uint8_t m_token[16];
static struct ctr_cloud_packet m_pck_send;
static struct ctr_cloud_packet m_pck_recv;
static ctr_cloud_transfer_cb m_cb;

static int transfer(struct ctr_cloud_packet *pck_send, struct ctr_cloud_packet *pck_recv, bool rai,
		    k_timeout_t timeout)
{
	int ret;
	struct ctr_buf *pck_buf = &m_buf_0;
	struct ctr_buf *send_buf = &m_buf_1;
	struct ctr_buf *recv_buf = &m_buf_0;
	size_t len;

	LOG_INF("Sending packet Sequence: %u %s len: %u", pck_send->sequence,
		ctr_cloud_packet_flags_to_str(pck_send->flags), pck_send->data_len);

	ctr_buf_reset(pck_buf);
	ret = ctr_cloud_packet_pack(pck_send, m_token, pck_buf);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_packet_pack` failed: %d", ret);
		return ret;
	}

	ctr_buf_reset(send_buf);
	ret = base64_encode(ctr_buf_get_mem(send_buf), ctr_buf_get_free(send_buf), &len,
			    ctr_buf_get_mem(pck_buf), ctr_buf_get_used(pck_buf));
	if (ret) {
		LOG_ERR("Call `base64_encode` failed: %d", ret);
		return ret;
	}
	ctr_buf_seek(send_buf, len);
	len = 0;

	LOG_HEXDUMP_INF(ctr_buf_get_mem(send_buf), ctr_buf_get_used(send_buf),
			rai ? "Sending packet RAI:" : "Sending packet:");

	struct ctr_lte_v2_send_recv_param param = {
		.rai = rai,
		.send_as_string = true,
		.send_buf = ctr_buf_get_mem(send_buf),
		.send_len = ctr_buf_get_used(send_buf),
		.recv_buf = NULL,
		.recv_size = 0,
		.recv_len = &len,
		.timeout = timeout,
	};

	if (pck_recv) {
		ctr_buf_reset(recv_buf);
		param.recv_buf = ctr_buf_get_mem(recv_buf);
		param.recv_size = ctr_buf_get_free(recv_buf);
	}

	ret = ctr_lte_v2_send_recv(&param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_send_recv` failed: %d", ret);
		return ret;
	}

	if (pck_recv) {
		ret = ctr_buf_seek(recv_buf, len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
			return ret;
		}

		if (!ctr_buf_get_used(recv_buf)) {
			LOG_ERR("No data received");
			return -EIO;
		}

		// LOG_HEXDUMP_INF(ctr_buf_get_mem(recv_buf), ctr_buf_get_used(recv_buf),
		// 		"Received packet:");

		pck_buf = &m_buf_1;
		len = 0;

		ctr_buf_reset(pck_buf);
		ret = base64_decode(ctr_buf_get_mem(pck_buf), ctr_buf_get_free(pck_buf), &len,
				    ctr_buf_get_mem(recv_buf), ctr_buf_get_used(recv_buf));
		if (ret) {
			LOG_ERR("Call `base64_decode` failed: %d", ret);
			return ret;
		}
		ctr_buf_seek(pck_buf, len);

		ret = ctr_cloud_packet_unpack(pck_recv, m_token, pck_buf);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_packet_unpack` failed: %d", ret);
			return ret;
		}

		LOG_INF("Received packet Sequence: %u %s len: %u", pck_recv->sequence,
			ctr_cloud_packet_flags_to_str(pck_recv->flags), pck_recv->data_len);
	}

	return 0;
}

int ctr_cloud_transfer_init(uint32_t serial_number, uint8_t token[16], ctr_cloud_transfer_cb cb)
{
	memset(&m_pck_send, 0, sizeof(m_pck_send));
	memset(&m_pck_recv, 0, sizeof(m_pck_recv));

	m_pck_send.serial_number = serial_number;
	memcpy(m_token, token, sizeof(m_token));

	m_sequence = 0;
	m_last_recv_sequence = 0;

	m_cb = cb;

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

int ctr_cloud_transfer_uplink(struct ctr_buf *buf, bool *has_downlink, k_timeout_t timeout)
{
	int ret = 0;
	int res = 0;
	uint8_t *p = NULL;
	size_t len = 0;
	int part = 0;
	int fragments = 0;

	if (has_downlink) {
		*has_downlink = false;
	}

restart:
	part = 0;

	if (buf) {
		p = ctr_buf_get_mem(buf);
		len = ctr_buf_get_used(buf);

		/* calculate number of fragments */
		fragments = len / MAX_BASE64_DATA_SIZE;
		if (len % MAX_BASE64_DATA_SIZE) {
			fragments++;
		}
	}

	do {
		LOG_INF("Processing part: %d (%d left)", part, fragments - part - 1);

		m_pck_send.data = p;
		m_pck_send.data_len = MIN(len, MAX_BASE64_DATA_SIZE);
		m_pck_send.sequence = m_sequence;
		m_sequence = ctr_cloud_packet_sequence_inc(m_sequence);

		m_pck_send.flags = 0;
		if (part == 0) {
			m_pck_send.flags |= CTR_CLOUD_PACKET_FLAG_FIRST;
		}
		if (len == m_pck_send.data_len) {
			m_pck_send.flags |= CTR_CLOUD_PACKET_FLAG_LAST;
		}

		bool rai = m_pck_send.flags & CTR_CLOUD_PACKET_FLAG_LAST;
		ret = transfer(&m_pck_send, &m_pck_recv, rai, timeout);
		if (ret) {
			LOG_ERR("Call `transfer` failed: %d", ret);
			res = ret;
			goto exit;
		}

		if (m_pck_recv.serial_number != m_pck_send.serial_number) {
			LOG_ERR("Serial number mismatch");
			res = -EREMCHG;
			goto exit;
		}

		if (has_downlink) {
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
				continue;
			} else {
				LOG_WRN("Received unexpected sequence expect: %u", m_sequence);
				m_sequence = 0;
				goto restart;
			}
		}

		if (m_pck_recv.data_len) {
			LOG_ERR("Received unexpected data length");
			m_sequence = 0;
			goto restart;
		}

		m_last_recv_sequence = m_pck_recv.sequence;
		m_sequence = ctr_cloud_packet_sequence_inc(m_sequence);

		p += m_pck_send.data_len;
		len -= m_pck_send.data_len;
		part++;

	} while (len);

exit:
	if (res) {
		LOG_ERR("Transfer uplink failed: %d reset sequence", res);
		m_sequence = 0;
		m_last_recv_sequence = 0;
	}

	if (m_cb) {
		struct ctr_cloud_transfer_event_data data = {
			.fragments = fragments,
			.bytes = ctr_buf_get_used(buf),
		};
		m_cb(res ? CTR_CLOUD_TRANSFER_EVENT_UPLINK_ERROR
			 : CTR_CLOUD_TRANSFER_EVENT_UPLINK_OK,
		     &data);
	}

	return res;
}

int ctr_cloud_transfer_downlink(struct ctr_buf *buf, bool *has_downlink, k_timeout_t timeout)
{
	int ret;
	int res = 0;
	int part = 0;
	bool quit;

	if (has_downlink) {
		*has_downlink = false;
	}
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
		if (quit) {
			bool rai = has_downlink ? !*has_downlink
						: true; /* use RAI if no downlink is expected */
			ret = transfer(&m_pck_send, NULL, rai, timeout);
			if (ret) {
				LOG_ERR("Call `transfer` failed: %d", ret);
				res = ret;
				goto exit;
			}
			break;
		}

		bool rai = part == 0;
		ret = transfer(&m_pck_send, &m_pck_recv, rai, timeout);
		if (ret) {
			LOG_ERR("Call `transfer` failed: %d", ret);
			res = ret;
			goto exit;
		}

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

		if (has_downlink) {
			*has_downlink = m_pck_recv.flags & CTR_CLOUD_PACKET_FLAG_POLL;
		}

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

	if (m_cb) {
		enum ctr_cloud_transfer_event event;
		if (res) {
			event = CTR_CLOUD_TRANSFER_EVENT_DOWNLINK_ERROR;
		} else if (part) {
			event = CTR_CLOUD_TRANSFER_EVENT_DOWNLINK_OK;
		} else {
			event = CTR_CLOUD_TRANSFER_EVENT_POLL;
		}
		struct ctr_cloud_transfer_event_data data = {
			.fragments = part,
			.bytes = ctr_buf_get_used(buf) - buf_used,
		};
		m_cb(event, &data);
	}

	return res;
}
