/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_state.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static K_MUTEX_DEFINE(m_lock);

static uint64_t m_imei;
static uint64_t m_imsi;
static char m_iccid[22 + 1] = {0};
static char m_fw_version[64] = {0};
static struct ctr_lte_v2_conn_param m_conn_param = {0};
static struct ctr_lte_v2_cereg_param m_cereg_param = {0};
static struct ctr_lte_v2_gnss_update m_gnss_update = {0};

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

	strncpy(m_iccid, iccid, sizeof(m_iccid));

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

	strncpy(m_fw_version, version, sizeof(m_fw_version));

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

	k_mutex_lock(&m_lock, K_FOREVER);

	memcpy(&m_cereg_param, param, sizeof(m_cereg_param));

	k_mutex_unlock(&m_lock);
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
