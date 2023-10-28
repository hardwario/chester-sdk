/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_therm.h>
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>

#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>

/* Standard includes */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

struct data {
	float therm_temperature;
	float hygro_temperature;
	float hygro_humidity;
};

static atomic_t m_measure = true;
static atomic_t m_send = true;
static bool m_lte_eval_valid;
static K_MUTEX_DEFINE(m_lte_eval_mut);
static K_SEM_DEFINE(m_loop_sem, 1, 1);
static K_SEM_DEFINE(m_run_sem, 0, 1);
static struct ctr_lte_eval m_lte_eval;
static struct data m_data = {
	.therm_temperature = NAN,
	.hygro_temperature = NAN,
	.hygro_humidity = NAN,
};
static struct ctr_wdog_channel m_wdog_channel;

static void start(void)
{
	int ret;

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}
}

static void attach(void)
{
	int ret;

	ret = ctr_lte_attach(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		k_oops();
	}
}

static void lte_event_handler(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param)
{
	switch (event) {
	case CTR_LTE_EVENT_FAILURE:
		LOG_ERR("Event `CTR_LTE_EVENT_FAILURE`");
		start();
		break;

	case CTR_LTE_EVENT_START_OK:
		LOG_INF("Event `CTR_LTE_EVENT_START_OK`");
		attach();
		break;

	case CTR_LTE_EVENT_START_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_START_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_ATTACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_ATTACH_OK`");
		k_sem_give(&m_run_sem);
		break;

	case CTR_LTE_EVENT_ATTACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_ATTACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_DETACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_DETACH_OK`");
		break;

	case CTR_LTE_EVENT_DETACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_DETACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_EVAL_OK:
		LOG_INF("Event `CTR_LTE_EVENT_EVAL_OK`");

		struct ctr_lte_eval *eval = &data->eval_ok.eval;

		LOG_DBG("EEST: %d", eval->eest);
		LOG_DBG("ECL: %d", eval->ecl);
		LOG_DBG("RSRP: %d", eval->rsrp);
		LOG_DBG("RSRQ: %d", eval->rsrq);
		LOG_DBG("SNR: %d", eval->snr);
		LOG_DBG("PLMN: %d", eval->plmn);
		LOG_DBG("CID: %d", eval->cid);
		LOG_DBG("BAND: %d", eval->band);
		LOG_DBG("EARFCN: %d", eval->earfcn);

		k_mutex_lock(&m_lte_eval_mut, K_FOREVER);
		memcpy(&m_lte_eval, &data->eval_ok.eval, sizeof(m_lte_eval));
		m_lte_eval_valid = true;
		k_mutex_unlock(&m_lte_eval_mut);

		break;

	case CTR_LTE_EVENT_EVAL_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_EVAL_ERR`");

		k_mutex_lock(&m_lte_eval_mut, K_FOREVER);
		m_lte_eval_valid = false;
		k_mutex_unlock(&m_lte_eval_mut);

		break;

	case CTR_LTE_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LTE_EVENT_SEND_OK`");
		break;

	case CTR_LTE_EVENT_SEND_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_SEND_ERR`");
		start();
		break;

	default:
		LOG_WRN("Unknown event: %d", event);
		return;
	}
}

static int compose(struct ctr_buf *buf)
{
	int ret;
	int err = 0;

	ctr_buf_reset(buf);
	err |= ctr_buf_seek(buf, 8);

	static uint32_t sequence;
	err |= ctr_buf_append_u32_le(buf, sequence++);

	uint8_t protocol = 1;
	err |= ctr_buf_append_u8(buf, protocol);

	uint32_t uptime = k_uptime_get() / 1000;
	err |= ctr_buf_append_u32_le(buf, uptime);

	uint64_t imei;
	ret = ctr_lte_get_imei(&imei);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imei` failed: %d", ret);
		return ret;
	}

	err |= ctr_buf_append_u64_le(buf, imei);

	uint64_t imsi;
	ret = ctr_lte_get_imsi(&imsi);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
		return ret;
	}

	err |= ctr_buf_append_u64_le(buf, imsi);

	k_mutex_lock(&m_lte_eval_mut, K_FOREVER);

	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.eest);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.ecl);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.rsrp);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.rsrq);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.snr);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.plmn);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.cid);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.band);
	err |= ctr_buf_append_s32_le(buf, !m_lte_eval_valid ? INT32_MAX : m_lte_eval.earfcn);

	m_lte_eval_valid = false;

	k_mutex_unlock(&m_lte_eval_mut);

	if (isnan(m_data.therm_temperature)) {
		err |= ctr_buf_append_s16_le(buf, INT16_MAX);
	} else {
		err |= ctr_buf_append_s16_le(buf, m_data.therm_temperature * 10.f);
	}

	if (isnan(m_data.hygro_temperature)) {
		err |= ctr_buf_append_s16_le(buf, INT16_MAX);
	} else {
		err |= ctr_buf_append_s16_le(buf, m_data.hygro_temperature * 10.f);
	}

	if (isnan(m_data.hygro_humidity)) {
		err |= ctr_buf_append_u16_le(buf, UINT16_MAX);
	} else {
		err |= ctr_buf_append_u16_le(buf, m_data.hygro_humidity * 10.f);
	}

	size_t len = ctr_buf_get_used(buf);

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

	err |= ctr_buf_seek(buf, 0);
	err |= ctr_buf_append_u16_le(buf, len);
	err |= ctr_buf_append_mem(buf, digest, 6);
	err |= ctr_buf_seek(buf, len);

	return err ? -EFAULT : 0;
}

static void send_timer(struct k_timer *timer_id)
{
	LOG_INF("Send timer expired");

	atomic_set(&m_send, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_send_timer, send_timer, NULL);

static int send(void)
{
	int ret;

	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.report_interval * 1000 / 5);
	int64_t duration = g_app_config.report_interval * 1000 + jitter;

	LOG_INF("Scheduling next report in %lld second(s)", duration / 1000);

	k_timer_start(&m_send_timer, Z_TIMEOUT_MS(duration), K_FOREVER);

	CTR_BUF_DEFINE_STATIC(buf, 1024);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 4101;
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void measure_timer(struct k_timer *timer_id)
{
	LOG_INF("Measurement timer expired");

	atomic_set(&m_measure, true);
	k_sem_give(&m_loop_sem);
}

static K_TIMER_DEFINE(m_measure_timer, measure_timer, NULL);

static int measure(void)
{
	int ret;

	k_timer_start(&m_measure_timer, Z_TIMEOUT_MS(g_app_config.measurement_interval * 1000),
		      K_FOREVER);

#if defined(CONFIG_CTR_THERM)
	ret = ctr_therm_read(&m_data.therm_temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		m_data.therm_temperature = NAN;
	}
#endif /* defined(CONFIG_CTR_THERM) */

#if defined(CONFIG_SHIELD_CTR_S2)
	ret = ctr_hygro_read(&m_data.hygro_temperature, &m_data.hygro_humidity);
	if (ret) {
		LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);
		m_data.hygro_temperature = NAN;
		m_data.hygro_humidity = NAN;
	}
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

	return 0;
}

static int loop(void)
{
	int ret;

	if (atomic_get(&m_measure)) {
		atomic_set(&m_measure, false);

		ret = measure();
		if (ret) {
			LOG_ERR("Call `measure` failed: %d", ret);
			return ret;
		}
	}

	if (atomic_get(&m_send)) {
		atomic_set(&m_send, false);

		ret = send();
		if (ret) {
			LOG_ERR("Call `send` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

int main(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_install(&m_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lte_set_event_cb(lte_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		ret = ctr_wdog_feed(&m_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			k_oops();
		}

		ret = k_sem_take(&m_run_sem, K_FOREVER);
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			k_oops();
		}

		break;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	for (;;) {
		LOG_INF("Alive");

		ret = ctr_wdog_feed(&m_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			k_oops();
		}

		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(30));
		ctr_led_set(CTR_LED_CHANNEL_G, false);

		ret = k_sem_take(&m_loop_sem, K_SECONDS(5));
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			k_oops();
		}

		ret = loop();
		if (ret) {
			LOG_ERR("Call `loop` failed: %d", ret);
			k_oops();
		}
	}

	return 0;
}

static int cmd_measure(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = measure();
	if (ret) {
		LOG_ERR("Call `measure` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_send(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	ret = send();
	if (ret) {
		LOG_ERR("Call `send` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

SHELL_CMD_REGISTER(measure, NULL, "Start measurement immediately.", cmd_measure);
SHELL_CMD_REGISTER(send, NULL, "Send data immediately.", cmd_send);
