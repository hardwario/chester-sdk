/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
 * Mock transport: records every uplink (the exact bytes of m_transfer_buf handed over by
 * ctr_cloud.c) and answers the session request, so that ctr_cloud_init() completes.
 */

#include "common.h"
#include "ctr_cloud_msg.h"
#include "ctr_cloud_transfer.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <string.h>

/* DL_SET_SESSION: id 0x64ba538f, timestamp 0x64ba538e, device id and name
 * (payload of the "download session" packet in ../ctr_cloud/src/test_cloud.c) */
static const uint8_t m_session_msg[] = {
	0x80, 0xa4, 0x00, 0x1a, 0x64, 0xba, 0x53, 0x8f, 0x04, 0x1a, 0x64, 0xba, 0x53, 0x8e, 0x05,
	0x78, 0x24, '0',  '1',  '8',  '8',  'd',  '8',  '5',  'c',  '-',  '8',  '9',  '6',  '2',
	'-',  '7',  '8',  '6',  'f',  '-',  'c',  '9',  '8',  '5',  '-',  'a',  '6',  '7',  'f',
	'e',  'e',  '7',  'b',  'b',  '1',  '4',  '4',  0x06, 0x72, 't',  'e',  's',  't',  '-',
	'd',  'e',  'v',  'i',  'c',  'e',  '-',  's',  '0',  '-',  '9',  '8',  '5',
};

static int m_init_ret;
static bool m_session_pending;

static int m_uplink_count;
static struct mock_uplink m_uplinks[MOCK_UPLINK_MAX];
static int m_fail_next_uplink;
static bool m_gate;
static int m_downlink_count;

static int m_log_count;
static struct mock_log_entry m_log[MOCK_LOG_MAX];
/* Error of the first uplinks of each message type (UL_*), and how many of them fail */
static int m_fail_type_err[UL_UPLOAD_FIRMWARE + 1];
static int m_fail_type_count[UL_UPLOAD_FIRMWARE + 1];
static bool m_session_without_id;

static K_SEM_DEFINE(m_entered, 0, 1);
static K_SEM_DEFINE(m_release, 0, 1);

static void release_timer_handler(struct k_timer *timer)
{
	k_sem_give(&m_release);
}

static K_TIMER_DEFINE(m_release_timer, release_timer_handler, NULL);

void mock_transfer_reset(void)
{
	m_uplink_count = 0;
	memset(m_uplinks, 0, sizeof(m_uplinks));
	m_fail_next_uplink = 0;
	m_gate = false;
	m_downlink_count = 0;
	k_timer_stop(&m_release_timer);
	k_sem_reset(&m_entered);
	k_sem_reset(&m_release);
}

void mock_transfer_set_init_ret(int ret)
{
	m_init_ret = ret;
}

void mock_transfer_fail_next_uplink(int err)
{
	m_fail_next_uplink = err;
}

void mock_transfer_gate_next_uplink(void)
{
	m_gate = true;
}

int mock_transfer_wait_entered(k_timeout_t timeout)
{
	return k_sem_take(&m_entered, timeout);
}

void mock_transfer_release(void)
{
	k_sem_give(&m_release);
}

void mock_transfer_release_after(k_timeout_t delay)
{
	k_timer_start(&m_release_timer, delay, K_NO_WAIT);
}

int mock_transfer_uplink_count(void)
{
	return m_uplink_count;
}

const struct mock_uplink *mock_transfer_uplink(int index)
{
	if (index < 0 || index >= MIN(m_uplink_count, MOCK_UPLINK_MAX)) {
		return NULL;
	}

	return &m_uplinks[index];
}

int mock_transfer_downlink_count(void)
{
	return m_downlink_count;
}

int mock_transfer_log_count(void)
{
	return m_log_count;
}

const struct mock_log_entry *mock_transfer_log(int index)
{
	if (index < 0 || index >= MIN(m_log_count, MOCK_LOG_MAX)) {
		return NULL;
	}

	return &m_log[index];
}

int mock_transfer_log_find(uint8_t type)
{
	for (int i = 0; i < MIN(m_log_count, MOCK_LOG_MAX); i++) {
		if (m_log[i].len && m_log[i].mem[0] == type) {
			return i;
		}
	}

	return -1;
}

void mock_transfer_fail_first_of_type(uint8_t type, int err, int count)
{
	__ASSERT_NO_MSG(type < ARRAY_SIZE(m_fail_type_err));

	m_fail_type_err[type] = err;
	m_fail_type_count[type] = count;
}

void mock_transfer_session_without_id_first(void)
{
	m_session_without_id = true;
}

int ctr_cloud_transfer_init(uint32_t serial_number, uint8_t token[16], ctr_cloud_transfer_cb cb)
{
	return m_init_ret;
}

int ctr_cloud_transfer_wait_for_ready(k_timeout_t timeout)
{
	return 0;
}

int ctr_cloud_transfer_uplink(struct ctr_buf *buf, bool *has_downlink, k_timeout_t timeout)
{
	const uint8_t *mem = ctr_buf_get_mem(buf);
	size_t len = ctr_buf_get_used(buf);
	int ret = 0;

	if (m_gate) {
		m_gate = false;
		k_sem_give(&m_entered);
		k_sem_take(&m_release, K_FOREVER);
	}

	struct mock_log_entry *e = NULL;

	if (m_log_count < MOCK_LOG_MAX) {
		e = &m_log[m_log_count];
		e->len = len;
		memcpy(e->mem, mem, MIN(len, sizeof(e->mem)));
		e->at = k_uptime_get();
	}

	m_log_count++;

	if (len && mem[0] == UL_CREATE_SESSION) {
		m_session_pending = true;
		*has_downlink = true;
		return 0;
	}

	if (m_fail_next_uplink) {
		ret = m_fail_next_uplink;
		m_fail_next_uplink = 0;
	}

	if (len && mem[0] < ARRAY_SIZE(m_fail_type_count) && m_fail_type_count[mem[0]]) {
		ret = m_fail_type_err[mem[0]];
		m_fail_type_count[mem[0]]--;
	}

	if (e) {
		e->ret = ret;
	}

	if (m_uplink_count < MOCK_UPLINK_MAX) {
		struct mock_uplink *u = &m_uplinks[m_uplink_count];

		u->len = len;
		memcpy(u->mem, mem, MIN(len, sizeof(u->mem)));
		u->timeout = timeout;
		u->ret = ret;
	}

	m_uplink_count++;

	return ret;
}

int ctr_cloud_transfer_downlink(struct ctr_buf *buf, bool *has_downlink, k_timeout_t timeout)
{
	*has_downlink = false;

	if (m_session_pending) {
		m_session_pending = false;

		if (m_session_without_id) {
			m_session_without_id = false;

			/* The map with one pair less: without the id (key 0) */
			const uint8_t head[] = {m_session_msg[0], 0xa3};
			int ret = ctr_buf_append_mem(buf, head, sizeof(head));

			if (ret) {
				return ret;
			}

			return ctr_buf_append_mem(buf, &m_session_msg[8],
						  sizeof(m_session_msg) - 8);
		}

		return ctr_buf_append_mem(buf, m_session_msg, sizeof(m_session_msg));
	}

	m_downlink_count++;

	return 0;
}
