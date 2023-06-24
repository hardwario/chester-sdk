/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"
#include "app_measure.h"
#include "app_send.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>

#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

static int compose(struct ctr_buf *buf)
{
	int ret;

	ctr_buf_reset(buf);
	ret = ctr_buf_seek(buf, 8);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_info_get_serial_number_uint32(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u32_le(buf, serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u32_le` failed: %d", ret);
		return ret;
	}

	zcbor_state_t zs[8];
	zcbor_new_state(zs, ARRAY_SIZE(zs), ctr_buf_get_mem(buf) + 12, ctr_buf_get_free(buf), 0);

	ret = app_cbor_encode(zs);
	if (ret) {
		LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
		return ret;
	}

	size_t len = zs[0].payload_mut - ctr_buf_get_mem(buf);

	struct tc_sha256_state_struct s;
	ret = tc_sha256_init(&s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_init` failed: %d", ret);
		return ret;
	}

	ret = tc_sha256_update(&s, ctr_buf_get_mem(buf) + 8, len - 8);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_update` failed: %d", ret);
		return ret;
	}

	uint8_t digest[32];
	ret = tc_sha256_final(digest, &s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_final` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, 0);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u16_le(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u16_le` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_mem(buf, digest, 6);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void send_timer(struct k_timer *timer_id)
{
	LOG_INF("Send timer expired");

	atomic_set(&g_app_loop_send, true);
	k_sem_give(&g_app_loop_sem);
}

K_TIMER_DEFINE(g_app_send_timer, send_timer, NULL);

int app_send(void)
{
	int ret;

	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.report_interval * 1000 / 5);
	int64_t duration = g_app_config.report_interval * 1000 + jitter;

	LOG_INF("Scheduling next report in %lld second(s)", duration / 1000);

	k_timer_start(&g_app_send_timer, K_MSEC(duration), K_FOREVER);

	ret = app_measure();
	if (ret) {
		LOG_ERR("Call `app_measure` failed: %d", ret);
		return ret;
	}

	CTR_BUF_DEFINE_STATIC(buf, 1024);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		if (ret == -EFAULT) {
			g_app_data.weight_measurement_count = 0;
#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
			g_app_data.people_measurement_count = 0;
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */
		}
		return ret;
	}

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 5000;
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	g_app_data.weight_measurement_count = 0;

	ret = ctr_rtc_get_ts(&g_app_data.weight_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Set base timestamp for weight measurement: %llu",
		g_app_data.weight_measurement_timestamp);

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	g_app_data.people_measurement_count = 0;

	ret = ctr_rtc_get_ts(&g_app_data.people_measurement_timestamp);
	if (ret) {
		LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Set base timestamp for people measurement: %llu",
		g_app_data.people_measurement_timestamp);
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

	return 0;
}
