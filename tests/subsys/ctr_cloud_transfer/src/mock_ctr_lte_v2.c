/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "mock.h"

#include "ctr_cloud_packet.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/sys/base64.h>
#include <zephyr/ztest.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CALLS_MAX 16

CTR_BUF_DEFINE_STATIC(m_pck_buf, CTR_CLOUD_PACKET_MAX_SIZE);

static uint32_t m_serial_number;
static uint8_t m_token[16];
static const struct mock_lte_exchange *m_exchanges;
static size_t m_count;
static size_t m_index;
static struct mock_lte_call m_calls[CALLS_MAX];

void mock_lte_start(uint32_t serial_number, const uint8_t token[16],
		    const struct mock_lte_exchange *exchanges, size_t count)
{
	zassert_true(count <= CALLS_MAX, "script of %zu exchanges, at most %d", count, CALLS_MAX);

	m_serial_number = serial_number;
	memcpy(m_token, token, sizeof(m_token));
	m_exchanges = exchanges;
	m_count = count;
	m_index = 0;
	memset(m_calls, 0, sizeof(m_calls));
}

const struct mock_lte_call *mock_lte_call(size_t index)
{
	zassert_true(index < m_index, "call %zu not made, %zu calls", index, m_index);

	return &m_calls[index];
}

void mock_lte_assert_done(void)
{
	zassert_equal(m_index, m_count, "script stopped at exchange %zu of %zu", m_index, m_count);
}

int ctr_lte_v2_enable(void)
{
	return 0;
}

int ctr_lte_v2_wait_for_connected(k_timeout_t timeout)
{
	return 0;
}

static int64_t timeout_ms(k_timeout_t timeout)
{
	if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
		return MOCK_LTE_FOREVER;
	}

	return k_ticks_to_ms_floor64(timeout.ticks);
}

int ctr_lte_v2_send_recv(const struct ctr_lte_v2_send_recv_param *param)
{
	size_t i = m_index;

	zassert_true(i < m_count, "send_recv %zu past the end of the script", i);

	const struct mock_lte_exchange *ex = &m_exchanges[i];
	struct mock_lte_call *call = &m_calls[i];

	m_index++;

	call->start = k_uptime_get();
	call->timeout_ms = timeout_ms(param->timeout);

	/* The packet sent */
	size_t len;
	ctr_buf_reset(&m_pck_buf);
	zassert_ok(base64_decode(ctr_buf_get_mem(&m_pck_buf), ctr_buf_get_free(&m_pck_buf), &len,
				 param->send_buf, param->send_len));
	ctr_buf_seek(&m_pck_buf, len);

	struct ctr_cloud_packet pck;
	zassert_ok(ctr_cloud_packet_unpack(&pck, m_token, &m_pck_buf));
	zassert_equal(pck.sequence, ex->sequence, "exchange %zu: sequence %u, expected %u", i,
		      pck.sequence, ex->sequence);
	zassert_equal(pck.flags, ex->flags, "exchange %zu: flags 0x%x, expected 0x%x", i, pck.flags,
		      ex->flags);
	zassert_equal(param->recv_buf == NULL, ex->no_recv, "exchange %zu: recv_buf %p", i,
		      param->recv_buf);

	/* The call takes its time, but not more than its timeout */
	k_timepoint_t end = sys_timepoint_calc(param->timeout);
	k_timepoint_t done = end;

	if (ex->duration_ms != MOCK_LTE_UNTIL_TIMEOUT) {
		done = sys_timepoint_calc(K_MSEC(ex->duration_ms));
		if (sys_timepoint_cmp(done, end) > 0) {
			done = end;
		}
	} else {
		zassert_false(K_TIMEOUT_EQ(param->timeout, K_FOREVER),
			      "exchange %zu: until K_FOREVER", i);
	}

	k_sleep(sys_timepoint_timeout(done));

	if (ex->ret) {
		return ex->ret;
	}

	*param->recv_len = 0;

	if (param->recv_buf) {
		struct ctr_cloud_packet resp = {
			.serial_number = m_serial_number,
			.sequence = ex->resp_sequence,
			.flags = ex->resp_flags,
			.data = (uint8_t *)ex->resp_data,
			.data_len = ex->resp_data ? strlen(ex->resp_data) : 0,
		};

		zassert_ok(ctr_cloud_packet_pack(&resp, m_token, &m_pck_buf));
		zassert_ok(base64_encode(param->recv_buf, param->recv_size, &len,
					 ctr_buf_get_mem(&m_pck_buf),
					 ctr_buf_get_used(&m_pck_buf)));
		*param->recv_len = len;
	}

	return 0;
}
