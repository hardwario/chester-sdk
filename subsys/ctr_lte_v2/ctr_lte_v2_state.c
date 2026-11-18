/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_state.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static K_MUTEX_DEFINE(m_lock);

static uint64_t m_imei;
static uint64_t m_imsi;
static char m_iccid[22 + 1] = {0};
static char m_fw_version[64] = {0};
static char m_ceer[CTR_LTE_V2_STATE_CEER_SIZE] = {0};
static struct ctr_lte_v2_conn_param m_conn_param = {0};
static struct ctr_lte_v2_cereg_param m_cereg_param = {0};
static struct ctr_lte_v2_gnss_update m_gnss_update = {0};

#if CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0
/* Ring of CEREG changes; m_cereg_head is the next slot to write */
static struct ctr_lte_v2_cereg_event m_cereg_history[CONFIG_CTR_LTE_V2_CEREG_HISTORY];
static size_t m_cereg_head;
static size_t m_cereg_len;
#endif

static struct ctr_lte_v2_cereg_event m_last_reject;
static bool m_last_reject_valid;

static bool m_radio_off;

int ctr_lte_v2_state_get_imei(uint64_t *imei)
{
	if (!imei) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	*imei = m_imei;

	k_mutex_unlock(&m_lock);

	return *imei ? 0 : -ENODATA;
}

void ctr_lte_v2_state_set_imei(uint64_t imei)
{
	k_mutex_lock(&m_lock, K_FOREVER);

	m_imei = imei;

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_imsi(uint64_t *imsi)
{
	if (!imsi) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	*imsi = m_imsi;

	k_mutex_unlock(&m_lock);

	return *imsi ? 0 : -ENODATA;
}

void ctr_lte_v2_state_set_imsi(uint64_t imsi)
{

	k_mutex_lock(&m_lock, K_FOREVER);

	m_imsi = imsi;

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_iccid(char **iccid)
{
	if (!iccid) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	*iccid = m_iccid;

	k_mutex_unlock(&m_lock);

	return *iccid[0] != 0 ? 0 : -ENODATA;
}

void ctr_lte_v2_state_set_iccid(const char *iccid)
{
	if (!iccid) {
		return;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	strncpy(m_iccid, iccid, sizeof(m_iccid) - 1);
	m_iccid[sizeof(m_iccid) - 1] = '\0';

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_modem_fw_version(char **version)
{
	if (!version) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	*version = m_fw_version;

	k_mutex_unlock(&m_lock);

	return m_fw_version[0] != 0 ? 0 : -ENODATA;
}

void ctr_lte_v2_state_set_modem_fw_version(const char *version)
{
	if (!version) {
		return;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	strncpy(m_fw_version, version, sizeof(m_fw_version) - 1);
	m_fw_version[sizeof(m_fw_version) - 1] = '\0';

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_ceer(char *buf, size_t size)
{
	if (!buf || !size) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	/* A copy: the FSM may store a new report meanwhile */
	strncpy(buf, m_ceer, size - 1);
	buf[size - 1] = '\0';

	k_mutex_unlock(&m_lock);

	return buf[0] != 0 ? 0 : -ENODATA;
}

void ctr_lte_v2_state_set_ceer(const char *ceer)
{
	if (!ceer) {
		return;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	strncpy(m_ceer, ceer, sizeof(m_ceer) - 1);
	m_ceer[sizeof(m_ceer) - 1] = '\0';

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_conn_param(struct ctr_lte_v2_conn_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	memcpy(param, &m_conn_param, sizeof(m_conn_param));

	k_mutex_unlock(&m_lock);

	return 0;
}

void ctr_lte_v2_state_set_conn_param(const struct ctr_lte_v2_conn_param *param)
{
	if (!param) {
		return;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	memcpy(&m_conn_param, param, sizeof(m_conn_param));

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_cereg_param(struct ctr_lte_v2_cereg_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	memcpy(param, &m_cereg_param, sizeof(m_cereg_param));

	k_mutex_unlock(&m_lock);

	return 0;
}

void ctr_lte_v2_state_set_cereg_param(const struct ctr_lte_v2_cereg_param *param)
{
	if (!param) {
		return;
	}

	bool registered = param->stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME ||
			  param->stat == CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING;

	k_mutex_lock(&m_lock, K_FOREVER);

	/* The "+CEREG: 0" of our own CFUN=4 says nothing about the network */
	if (registered || !m_radio_off) {
		memcpy(&m_cereg_param, param, sizeof(m_cereg_param));
	}

	k_mutex_unlock(&m_lock);
}

void ctr_lte_v2_state_set_radio_off(bool off)
{
	k_mutex_lock(&m_lock, K_FOREVER);
	m_radio_off = off;
	k_mutex_unlock(&m_lock);
}

#if CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0
static bool cereg_event_same(const struct ctr_lte_v2_cereg_event *a,
			     const struct ctr_lte_v2_cereg_event *b)
{
	return a->stat == b->stat && a->act == b->act && a->tac == b->tac && a->cid == b->cid &&
	       a->reject_cause == b->reject_cause;
}
#endif

void ctr_lte_v2_state_add_cereg_event(const struct ctr_lte_v2_cereg_param *param)
{
	if (!param) {
		return;
	}

	struct ctr_lte_v2_cereg_event event = {
		.uptime_s = k_uptime_seconds(),
		.cid = param->cid,
		.tac = strtoul(param->tac, NULL, 16),
		.stat = param->stat,
		.act = param->act,
		/* cause_type 1 is manufacturer specific, not an EMM cause */
		.reject_cause = param->cause_type == 0 ? param->reject_cause : 0,
	};

	k_mutex_lock(&m_lock, K_FOREVER);

	if (ctr_lte_v2_state_cereg_is_reject(param)) {
		m_last_reject = event;
		m_last_reject_valid = true;
	}

#if CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0
	/* Repeats (e.g. on every TAU) would push out the real changes */
	size_t newest =
		(m_cereg_head + ARRAY_SIZE(m_cereg_history) - 1) % ARRAY_SIZE(m_cereg_history);
	if (!m_cereg_len || !cereg_event_same(&m_cereg_history[newest], &event)) {
		m_cereg_history[m_cereg_head] = event;
		m_cereg_head = (m_cereg_head + 1) % ARRAY_SIZE(m_cereg_history);
		m_cereg_len = MIN(m_cereg_len + 1, ARRAY_SIZE(m_cereg_history));
	}
#endif

	k_mutex_unlock(&m_lock);
}

int ctr_lte_v2_state_get_cereg_history(struct ctr_lte_v2_cereg_event *events, size_t max,
				       size_t *count)
{
	if (!events || !count) {
		return -EINVAL;
	}

#if CONFIG_CTR_LTE_V2_CEREG_HISTORY > 0
	k_mutex_lock(&m_lock, K_FOREVER);

	*count = MIN(max, m_cereg_len);
	for (size_t i = 0; i < *count; i++) {
		size_t idx = (m_cereg_head + ARRAY_SIZE(m_cereg_history) - 1 - i) %
			     ARRAY_SIZE(m_cereg_history);
		events[i] = m_cereg_history[idx];
	}

	k_mutex_unlock(&m_lock);
#else
	*count = 0;
#endif

	return 0;
}

int ctr_lte_v2_state_get_last_reject(struct ctr_lte_v2_cereg_event *event)
{
	if (!event) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	*event = m_last_reject;
	bool valid = m_last_reject_valid;
	k_mutex_unlock(&m_lock);

	return valid ? 0 : -ENODATA;
}

int ctr_lte_v2_state_set_gnss_update(const struct ctr_lte_v2_gnss_update *update)
{
	if (!update) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	memcpy(&m_gnss_update, update, sizeof(m_gnss_update));

	k_mutex_unlock(&m_lock);

	return 0;
}

int ctr_lte_v2_state_get_gnss_update(struct ctr_lte_v2_gnss_update *update)
{
	if (!update) {
		return -EINVAL;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	memcpy(update, &m_gnss_update, sizeof(m_gnss_update));

	k_mutex_unlock(&m_lock);

	return 0;
}
