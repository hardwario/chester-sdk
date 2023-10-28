/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static K_MUTEX_DEFINE(m_lte_eval_mut);
static struct ctr_lte_eval m_lte_eval;
static bool m_lte_eval_valid;

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

static int send(void)
{
	int ret;

	CTR_BUF_DEFINE(buf, 128);

	uint64_t imsi;
	ret = ctr_lte_get_imsi(&imsi);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
		return -EBUSY;
	}

	ctr_buf_append_u64_le(&buf, imsi);

	static uint32_t counter;
	ctr_buf_append_u32_le(&buf, counter++);

	k_mutex_lock(&m_lte_eval_mut, K_FOREVER);

	if (m_lte_eval_valid) {
		ctr_buf_append_s32_le(&buf, m_lte_eval.eest);
		ctr_buf_append_s32_le(&buf, m_lte_eval.ecl);
		ctr_buf_append_s32_le(&buf, m_lte_eval.rsrp);
		ctr_buf_append_s32_le(&buf, m_lte_eval.rsrq);
		ctr_buf_append_s32_le(&buf, m_lte_eval.snr);
		ctr_buf_append_s32_le(&buf, m_lte_eval.plmn);
		ctr_buf_append_s32_le(&buf, m_lte_eval.cid);
		ctr_buf_append_s32_le(&buf, m_lte_eval.band);
		ctr_buf_append_s32_le(&buf, m_lte_eval.earfcn);
	}

	m_lte_eval_valid = false;

	k_mutex_unlock(&m_lte_eval_mut);

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

#if 1
	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
#else
	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS_PUBLIC_IP;
#endif

	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

int main(void)
{
	int ret;

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
		LOG_INF("Alive");

		ret = send();
		if (ret) {
			LOG_ERR("Call `send` failed: %d", ret);
		}

		k_sleep(K_SECONDS(60));
	}

	return 0;
}
