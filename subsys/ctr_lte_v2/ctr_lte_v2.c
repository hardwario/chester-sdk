/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_config.h"
#include "ctr_lte_v2_flow.h"
#include "ctr_lte_v2_state.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_v2, CONFIG_CTR_LTE_V2_LOG_LEVEL);

#define PREPARE_RETRY_COUNT   1
#define PREPARE_RETRY_DELAY   K_SECONDS(10)
#define SEND_RECV_RETRY_COUNT 3
#define SEND_RECV_RETRY_DELAY K_SECONDS(10)

static K_MUTEX_DEFINE(m_lock);

#define STATE_PREPARED_BIT 0
#define STATE_ATTACHED_BIT 1

static atomic_t m_state;

/* clang-format off */
#define ATTACH_RETRY_DELAYS {                                                                      \
	K_NO_WAIT, K_MINUTES(1),                                                                   \
	K_NO_WAIT, K_MINUTES(5),                                                                   \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(1), K_MINUTES(5),                                                                  \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(6), K_MINUTES(5),                                                                  \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(24), K_MINUTES(5),                                                                 \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(168), K_MINUTES(5),                                                                \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(168), K_MINUTES(5),                                                                \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(168), K_MINUTES(5),                                                                \
	K_MINUTES(5), K_MINUTES(45),                                                               \
                                                                                                   \
	K_HOURS(168), K_MINUTES(5),                                                                \
	K_MINUTES(5), K_MINUTES(45),                                                               \
}
/* clang-format on */

int ctr_lte_v2_get_imei(uint64_t *imei)
{
	return ctr_lte_v2_state_get_imei(imei);
}

int ctr_lte_v2_get_imsi(uint64_t *imsi)
{
	return ctr_lte_v2_state_get_imsi(imsi);
}

int ctr_lte_v2_get_iccid(char **iccid)
{
	return ctr_lte_v2_state_get_iccid(iccid);
}

int ctr_lte_v2_get_modem_fw_version(char **version)
{
	return ctr_lte_v2_state_get_modem_fw_version(version);
}

int ctr_lte_v2_is_prepared(bool *prepared)
{
	*prepared = atomic_test_bit(&m_state, STATE_PREPARED_BIT) ? true : false;
	return 0;
}

int ctr_lte_v2_is_attached(bool *attached)
{
	*attached = atomic_test_bit(&m_state, STATE_ATTACHED_BIT) ? true : false;
	return 0;
}

int ctr_lte_v2_start(void)
{
	int ret;

	ret = ctr_lte_v2_flow_start();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_start` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_stop(void)
{
	int ret;

	ret = ctr_lte_v2_flow_stop();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_stop` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_wait_on_modem_sleep(k_timeout_t delay)
{
	int ret;

	ret = ctr_lte_v2_flow_wait_on_modem_sleep(delay);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_wait_on_modem_sleep` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_v2_prepare(void)
{
	int ret;

	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	if (atomic_test_bit(&m_state, STATE_PREPARED_BIT)) {
		k_mutex_unlock(&m_lock);
		return 0;
	}

	ret = ctr_lte_v2_flow_prepare(PREPARE_RETRY_COUNT, PREPARE_RETRY_DELAY);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_prepare` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	atomic_set_bit(&m_state, STATE_PREPARED_BIT);

	k_mutex_unlock(&m_lock);

	return 0;
}

int ctr_lte_v2_attach(void)
{
	int ret;

	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	if (atomic_test_bit(&m_state, STATE_ATTACHED_BIT)) {
		k_mutex_unlock(&m_lock);
		return 0;
	}

	LOG_INF("Operation ATTACH started");

	const k_timeout_t attach_retry_delays[] = ATTACH_RETRY_DELAYS;
	BUILD_ASSERT(ARRAY_SIZE(attach_retry_delays) % 2 == 0, "ATTACH retry delays must be even");

	for (size_t i = 0; i < ARRAY_SIZE(attach_retry_delays); i += 2) {

		k_timeout_t delay = attach_retry_delays[i];
		k_timeout_t attach_timeout = attach_retry_delays[i + 1];

		if (!K_TIMEOUT_EQ(delay, K_NO_WAIT)) {
			LOG_INF("Waiting %lld minutes before attach retry",
				k_ticks_to_ms_floor64(delay.ticks) / MSEC_PER_SEC / 60);
			k_sleep(delay);
		}

		ret = ctr_lte_v2_flow_start();
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_start` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		ret = ctr_lte_v2_prepare();
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_prepare` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		ret = ctr_lte_v2_flow_attach(attach_timeout);
		if (ret) {
			LOG_WRN("Call `ctr_lte_v2_flow_attach` failed: %d", ret);

			atomic_clear_bit(&m_state,
					 STATE_PREPARED_BIT); // force prepare on next attach

			if (ret == -EAGAIN) {
				i = 0; // restart attach retry delays
				ctr_lte_v2_flow_stop();
				continue;
			}

			ret = ctr_lte_v2_flow_reset();
			if (ret) {
				LOG_ERR("Call `ctr_lte_v2_flow_reset` failed: %d", ret);
				k_mutex_unlock(&m_lock);
				return ret;
			}

		} else {
			LOG_INF("Operation ATTACH succeeded");

			atomic_set_bit(&m_state, STATE_ATTACHED_BIT);

			k_mutex_unlock(&m_lock);
			return 0;
		}

		ret = ctr_lte_v2_flow_stop();
		if (ret) {
			LOG_ERR("Call `ctr_lte_v2_flow_stop` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}
	}

	LOG_WRN("Operation ATTACH failed");

	return ret;
}

int ctr_lte_v2_detach(void)
{
	int ret;

	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	atomic_clear_bit(&m_state, STATE_ATTACHED_BIT);
	atomic_clear_bit(&m_state, STATE_PREPARED_BIT);

	ret = ctr_lte_v2_flow_reset();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_reset` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

int ctr_lte_v2_send_recv(const struct ctr_lte_v2_send_recv_param *param, bool rai)
{
	int ret;

	if (g_ctr_lte_v2_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	ret = ctr_lte_v2_attach();
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_attach` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	ret = ctr_lte_v2_flow_send_recv(SEND_RECV_RETRY_COUNT, SEND_RECV_RETRY_DELAY, param, rai);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_flow_send_recv` failed: %d", ret);
		ctr_lte_v2_detach();
		k_mutex_unlock(&m_lock);
		return ret;
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

int ctr_lte_v2_get_conn_param(struct ctr_lte_v2_conn_param *param)
{
	return ctr_lte_v2_state_get_conn_param(param);
}

int ctr_lte_v2_get_cereg_param(struct ctr_lte_v2_cereg_param *param)
{
	return ctr_lte_v2_state_get_cereg_param(param);
}

/* clang-format on */
BUILD_ASSERT(CONFIG_CTR_LTE_V2_INIT_PRIORITY > CONFIG_CTR_LTE_V2_CONFIG_INIT_PRIORITY,
	     "CONFIG_CTR_LTE_V2_INIT_PRIORITY must be higher than "
	     "CONFIG_CTR_LTE_V2_CONFIG_INIT_PRIORITY");
