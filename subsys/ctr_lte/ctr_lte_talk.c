/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_parse.h"
#include "ctr_lte_talk.h"

#include <chester/ctr_util.h>
#include <chester/drivers/ctr_lte_if.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_talk, CONFIG_CTR_LTE_LOG_LEVEL);

#define SEND_GUARD_TIME	   K_MSEC(50)
#define RESPONSE_TIMEOUT_S K_SECONDS(3)
#define RESPONSE_TIMEOUT_L K_SECONDS(30)

typedef bool (*handler_cb)(const char *s, void *param);
typedef int (*response_cb)(int idx, int count, const char *s, void *p1, void *p2, void *p3);

struct response_item {
	char *s;
	sys_snode_t node;
};

struct response {
	int max;
	int count;
	sys_slist_t list;
	char *result;
	bool started;
	bool finished;
};

static const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(ctr_lte_if));

static handler_cb m_handler_cb;
static ctr_lte_talk_event_cb m_event_cb;
static K_MUTEX_DEFINE(m_handler_mut);
static K_MUTEX_DEFINE(m_talk_mut);
static struct k_poll_signal m_response_sig;
static void *m_handler_param;

static void process_urc(const char *s)
{
	int ret;

	if (strlen(s) == 0) {
		return;
	}

	LOG_INF("URC: %s", s);

	if (strcmp(s, "Ready") == 0) {
		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_TALK_EVENT_BOOT);
		}

		return;
	}

	if (strcmp(s, "%XSIM: 1") == 0) {
		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_TALK_EVENT_SIM_CARD);
		}

		return;
	}

	if (strncmp(s, "%XTIME:", 7) == 0) {
		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_TALK_EVENT_TIME);
		}

		return;
	}

	if (strncmp(s, "+CEREG:", 7) == 0) {
		int stat;

		ret = ctr_lte_parse_cereg(s, &stat);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_parse_cereg` failed: %d", ret);
		}

		if (stat == 1 || stat == 5) {
			if (m_event_cb != NULL) {
				m_event_cb(CTR_LTE_TALK_EVENT_ATTACH);
			}

		} else {
			if (m_event_cb != NULL) {
				m_event_cb(CTR_LTE_TALK_EVENT_DETACH);
			}
		}

		return;
	}
}

static void recv_cb(const char *s)
{
	k_mutex_lock(&m_handler_mut, K_FOREVER);

	if (m_handler_cb == NULL) {
		k_mutex_unlock(&m_handler_mut);
		process_urc(s);
		return;
	}

	if (!m_handler_cb(s, m_handler_param)) {
		k_mutex_unlock(&m_handler_mut);
		process_urc(s);
		return;
	}

	k_mutex_unlock(&m_handler_mut);
}

static bool response_handler(const char *s, void *param)
{
	struct response *response = param;

	size_t len = strlen(s);

	if (!response->started) {
		if (len == 0) {
			response->started = true;

			if (response->max > 0) {
				LOG_DBG("Response started");
			} else {
				response->finished = true;
			}

			return true;
		}

		return false;
	}

	if (len == 0) {
		response->finished = true;

		if (response->max > 0) {
			LOG_DBG("Response finished");
		}

		return true;
	}

	if (response->finished) {
		if (response->result == NULL) {
			char *p = k_malloc(len + 1);

			if (p == NULL) {
				LOG_ERR("Call `k_malloc` failed");
				k_oops();
			}

			strcpy(p, s);

			response->result = p;

			k_poll_signal_raise(&m_response_sig, 0);

			LOG_INF("RSP: %s", s);
			return true;
		}

		LOG_INF("RSP (ignored): %s", s);
		return false;
	}

	if (response->count++ >= response->max) {
		LOG_INF("RSP (ignored): %s", s);
		return true;
	}

	struct response_item *item = k_malloc(sizeof(*item));

	if (item == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		k_oops();
	}

	char *p = k_malloc(len + 1);

	if (p == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		k_oops();
	}

	strcpy(p, s);

	item->s = p;

	sys_slist_append(&response->list, &item->node);

	LOG_INF("RSP: %s", s);
	return true;
}

static int gather_response(k_timeout_t timeout, struct response *response, int max)
{
	int ret;

	memset(response, 0, sizeof(*response));

	sys_slist_init(&response->list);

	response->max = max;

	k_poll_signal_reset(&m_response_sig);

	k_mutex_lock(&m_handler_mut, K_FOREVER);
	m_handler_cb = response_handler;
	m_handler_param = response;
	k_mutex_unlock(&m_handler_mut);

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
					 &m_response_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), timeout);

	k_mutex_lock(&m_handler_mut, K_FOREVER);
	m_handler_cb = NULL;
	m_handler_param = NULL;
	k_mutex_unlock(&m_handler_mut);

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	if (response->count > response->max) {
		LOG_ERR("Exceeded maximum response fragment count");
		return -EFBIG;
	}

	return 0;
}

static void release_response(struct response *response)
{
	if (response->result != NULL) {
		k_free(response->result);
	}

	struct response_item *item;

	SYS_SLIST_FOR_EACH_CONTAINER (&response->list, item, node) {
		if (item->s != NULL) {
			k_free(item->s);
		}

		k_free(item);
	}
}

static int talk_cmd(const char *fmt, ...)
{
	int ret;

	/* TODO Do not block long operation by mutex but return -EBUSY instead */
	k_mutex_lock(&m_talk_mut, K_FOREVER);
	k_sleep(SEND_GUARD_TIME);

	va_list ap;

	va_start(ap, fmt);
	ret = ctr_lte_if_send(dev, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_send` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	k_mutex_unlock(&m_talk_mut);
	return 0;
}

static int talk_cmd_ok(k_timeout_t timeout, const char *fmt, ...)
{
	int ret;

	/* TODO Do not block long operation by mutex but return -EBUSY instead */
	k_mutex_lock(&m_talk_mut, K_FOREVER);
	k_sleep(SEND_GUARD_TIME);

	va_list ap;

	va_start(ap, fmt);
	ret = ctr_lte_if_send(dev, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_send` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	struct response response;

	ret = gather_response(timeout, &response, 0);

	if (ret < 0) {
		LOG_ERR("Call `gather_response` failed: %d", ret);
		release_response(&response);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	if (strcmp(response.result, "OK") != 0) {
		LOG_ERR("Unexpected result: %s", response.result);
		release_response(&response);
		k_mutex_unlock(&m_talk_mut);
		return -ENOMSG;
	}

	release_response(&response);
	k_mutex_unlock(&m_talk_mut);
	return 0;
}

static int talk_cmd_response_ok(k_timeout_t timeout, response_cb cb, void *p1, void *p2, void *p3,
				const char *fmt, ...)
{
	int ret;

	/* TODO Do not block long operation by mutex but return -EBUSY instead */
	k_mutex_lock(&m_talk_mut, K_FOREVER);
	k_sleep(SEND_GUARD_TIME);

	va_list ap;

	va_start(ap, fmt);
	ret = ctr_lte_if_send(dev, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_send` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	struct response response;

	ret = gather_response(timeout, &response, 1);

	if (ret < 0) {
		LOG_ERR("Call `gather_response` failed: %d", ret);
		release_response(&response);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	if (strcmp(response.result, "OK") != 0) {
		LOG_ERR("Unexpected result: %s", response.result);
		release_response(&response);
		k_mutex_unlock(&m_talk_mut);
		return -ENOMSG;
	}

	if (cb != NULL) {
		int idx = 0;
		struct response_item *item;

		SYS_SLIST_FOR_EACH_CONTAINER (&response.list, item, node) {
			ret = cb(idx++, response.count, item->s, p1, p2, p3);
			if (ret < 0) {
				LOG_ERR("Call `cb` failed: %d", ret);
				release_response(&response);
				k_mutex_unlock(&m_talk_mut);
				return ret;
			}
		}
	}

	release_response(&response);
	k_mutex_unlock(&m_talk_mut);
	return 0;
}

static int talk_cmd_raw_ok(k_timeout_t timeout, const void *buf, size_t len, const char *fmt, ...)
{
	int ret;

	/* TODO Do not block long operation by mutex but return -EBUSY instead */
	k_mutex_lock(&m_talk_mut, K_FOREVER);
	k_sleep(SEND_GUARD_TIME);

	va_list ap;

	va_start(ap, fmt);
	ret = ctr_lte_if_send(dev, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_send` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	k_sleep(K_MSEC(100));

	ret = ctr_lte_if_send_raw(dev, buf, len);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_send_raw` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	k_sleep(K_MSEC(1500));

	char terminator[3 + 1];

	strcpy(terminator, "+++");

	ret = ctr_lte_if_send_raw(dev, terminator, strlen(terminator));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_send_raw` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	struct response response;

	ret = gather_response(timeout, &response, 0);

	if (ret < 0) {
		LOG_ERR("Call `gather_response` failed: %d", ret);
		release_response(&response);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	if (strcmp(response.result, "OK") != 0) {
		LOG_ERR("Unexpected result: %s", response.result);
		release_response(&response);
		k_mutex_unlock(&m_talk_mut);
		return -ENOMSG;
	}

	release_response(&response);
	k_mutex_unlock(&m_talk_mut);
	return 0;
}

int ctr_lte_talk_init(ctr_lte_talk_event_cb event_cb)
{
	int ret;

	m_event_cb = event_cb;

	k_poll_signal_init(&m_response_sig);

	ret = ctr_lte_if_init(dev, recv_cb);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_enable(void)
{
	int ret;

	ret = ctr_lte_if_enable(dev);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_enable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_disable(void)
{
	int ret;

	ret = ctr_lte_if_disable(dev);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_if_disable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_(const char *s)
{
	int ret;

	ret = talk_cmd("%s", s);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at(void)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_cclk_q_response_handler(int idx, int count, const char *s, void *p1, void *p2,
				      void *p3)
{
	char *buf = p1;
	size_t *size = p2;

	if (idx == 0 && count == 1) {
		if (strlen(s) >= *size) {
			return -ENOBUFS;
		}

		strcpy(buf, s);

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_cclk_q(char *buf, size_t size)
{
	int ret;

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_cclk_q_response_handler, buf, &size, NULL,
				   "AT+CCLK?");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_cimi_response_handler(int idx, int count, const char *s, void *p1, void *p2, void *p3)
{
	char *buf = p1;
	size_t *size = p2;

	if (idx == 0 && count == 1) {
		size_t len = strlen(s);

		/* TODO Verify IMSI can be 14-15 digits long only */
		if (len < 14 || len > 15) {
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)s[i])) {
				return -EINVAL;
			}
		}

		if (len >= *size) {
			return -ENOBUFS;
		}

		strcpy(buf, s);

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_cimi(char *buf, size_t size)
{
	int ret;

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_cimi_response_handler, buf, &size, NULL,
				   "AT+CIMI");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_ceppi(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CEPPI=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cereg(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CEREG=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cfun(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+CFUN=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cgauth(int p1, int *p2, const char *p3, const char *p4)
{
	int ret;

	if (p2 == NULL && p3 == NULL && p4 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGAUTH=%d", p1);

	} else if (p2 != NULL && p3 == NULL && p4 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGAUTH=%d,%d", p1, *p2);

	} else if (p2 != NULL && p3 != NULL && p4 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGAUTH=%d,%d,\"%s\"", p1, *p2, p3);

	} else if (p2 != NULL && p3 != NULL && p4 != NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGAUTH=%d,%d,\"%s\",\"%s\"", p1, *p2, p3,
				  p4);

	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cgdcont(int p1, const char *p2, const char *p3)
{
	int ret;

	if (p2 == NULL && p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGDCONT=%d", p1);

	} else if (p2 != NULL && p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGDCONT=%d,\"%s\"", p1, p2);

	} else if (p2 != NULL && p3 != NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGDCONT=%d,\"%s\",\"%s\"", p1, p2, p3);

	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cgerep(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CGEREP=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_cgsn_response_handler(int idx, int count, const char *s, void *p1, void *p2, void *p3)
{
	char *buf = p1;
	size_t *size = p2;

	if (idx == 0 && count == 1) {
		size_t len = strlen(s);

		/* TODO Verify IMEI can be 15-16 digits long only */
		if (len < 15 || len > 16) {
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)s[i])) {
				return -EINVAL;
			}
		}

		if (len >= *size) {
			return -ENOBUFS;
		}

		strcpy(buf, s);

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_cgsn(char *buf, size_t size)
{
	int ret;

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_cgsn_response_handler, buf, &size, NULL,
				   "AT+CGSN");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cmee(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CMEE=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cnec(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CNEC=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_coneval_response_handler(int idx, int count, const char *s, void *p1, void *p2,
				       void *p3)
{
	int ret;

	struct ctr_lte_eval *eval = p1;

	if (idx == 0 && count == 1) {
		long result;
		long energy_estimate;
		long rsrp;
		long rsrq;
		long snr;
		char cell_id[8 + 1];
		char plmn[6 + 1];
		long earfcn;
		long band;
		long ce_level;

		ret = ctr_lte_parse_coneval(s, &result, NULL, &energy_estimate, &rsrp, &rsrq, &snr,
					    cell_id, sizeof(cell_id), plmn, sizeof(plmn), NULL,
					    &earfcn, &band, NULL, &ce_level, NULL, NULL, NULL,
					    NULL);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_parse_coneval` failed: %d", ret);
			return -EINVAL;
		}

		if (result == 0) {
			uint8_t cell_id_buf[4] = {0};
			ret = ctr_hex2buf(cell_id, cell_id_buf, sizeof(cell_id_buf), false);
			if (ret < 0) {
				LOG_ERR("Call `ctr_hex2buf` (cell_id) failed: %d", ret);
				return -EINVAL;
			}

			eval->eest = energy_estimate;
			eval->ecl = ce_level;
			eval->rsrp = rsrp - 140;
			eval->rsrq = (rsrq - 39) / 2;
			eval->snr = snr - 24;
			eval->plmn = strtol(plmn, NULL, 10);
			eval->cid = sys_get_be32(cell_id_buf);
			eval->band = band;
			eval->earfcn = earfcn;

			return 0;
		}
	}

	return -EINVAL;
}

int ctr_lte_talk_at_coneval(struct ctr_lte_eval *eval)
{
	int ret;

	/* TODO Clarify response timeout requirements */
	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_L, at_coneval_response_handler, eval, NULL,
				   NULL, "AT%%CONEVAL");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cops(int p1, int *p2, const char *p3)
{
	int ret;

	if (p2 == NULL && p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+COPS=%d", p1);

	} else if (p2 != NULL && p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+COPS=%d,%d", p1, *p2);

	} else if (p2 != NULL && p3 != NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+COPS=%d,%d,\"%s\"", p1, *p2, p3);

	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cpsms(int *p1, const char *p2, const char *p3)
{
	int ret;

	if (p1 == NULL && p2 == NULL && p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CPSMS=");

	} else if (p1 != NULL && p2 == NULL && p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CPSMS=%d", *p1);

	} else if (p1 != NULL && p2 != NULL && p3 != NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CPSMS=%d,,,\"%s\",\"%s\"", *p1, p2, p3);

	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_crsm_176_response_handler(int idx, int count, const char *s, void *p1, void *p2,
					void *p3)
{
	char *buf = p1;

	if (idx == 0 && count == 1) {
		size_t len = strlen(s);

		if (len != 39 || strncmp(s, "+CRSM: 144,0,", 13)) {
			return -EINVAL;
		}

		if (s[13] != '"' || s[38] != '"') {
			return -EINVAL;
		}

		strncpy(buf, &s[14], 24);
		buf[24] = '\0';

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_crsm_176(char *buf, size_t size)
{
	int ret;

	if (size < 24 + 1) {
		return -ENOBUFS;
	}

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_crsm_176_response_handler, buf, &size,
				   NULL, "AT+CRSM=176,28539,0,0,12");
	if (ret) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_crsm_214_response_handler(int idx, int count, const char *s, void *p1, void *p2,
					void *p3)
{
	if (idx == 0 && count == 1) {
		if (strcmp(s, "+CRSM: 144,0,\"\"")) {
			return -EINVAL;
		}

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_crsm_214(void)
{
	int ret;

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_crsm_214_response_handler, NULL, NULL,
				   NULL, "AT+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\"");
	if (ret) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_cscon(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CSCON=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_hwversion_response_handler(int idx, int count, const char *s, void *p1, void *p2,
					 void *p3)
{
	char *buf = p1;
	size_t *size = p2;

	if (idx == 0 && count == 1) {
		if (strlen(s) >= *size) {
			return -ENOBUFS;
		}

		strcpy(buf, s);

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_hwversion(char *buf, size_t size)
{
	int ret;

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_hwversion_response_handler, buf, &size,
				   NULL, "AT%%HWVERSION");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_mdmev(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%MDMEV=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_rai(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%RAI=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_rel14feat(int p1, int p2, int p3, int p4, int p5)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%REL14FEAT=%d,%d,%d,%d,%d", p1, p2, p3, p4, p5);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_shortswver_response_handler(int idx, int count, const char *s, void *p1, void *p2,
					  void *p3)
{
	char *p = p1;
	size_t *size = p2;

	if (idx == 0 && count == 1) {
		if (strlen(s) >= *size) {
			return -ENOBUFS;
		}

		strcpy(p, s);

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_shortswver(char *buf, size_t size)
{
	int ret;

	ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_shortswver_response_handler, buf, &size,
				   NULL, "AT%%SHORTSWVER");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xbandlock(int p1, const char *p2)
{
	int ret;

	if (p2 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XBANDLOCK=%d", p1);

	} else {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XBANDLOCK=%d,\"%s\"", p1, p2);
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xdataprfl(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XDATAPRFL=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xnettime(int p1, int *p2)
{
	int ret;

	if (p2 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XNETTIME=%d", p1);

	} else {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XNETTIME=%d,%d", p1, *p2);
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xpofwarn(int p1, int p2)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XPOFWARN=%d,%d", p1, p2);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xsendto(const char *p1, int p2, const void *buf, size_t len)
{
	int ret;

	ret = talk_cmd_raw_ok(RESPONSE_TIMEOUT_L, buf, len, "AT#XSENDTO=\"%s\",%d", p1, p2);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_raw_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xsocketopt(int p1, int p2, int *p3)
{
	int ret;

	if (p3 == NULL) {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT#XSOCKETOPT=%d,%d", p1, p2);

	} else {
		ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT#XSOCKETOPT=%d,%d,%d", p1, p2, *p3);
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xsim(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XSIM=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xsleep(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT#XSLEEP=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int at_xsocket_response_handler(int idx, int count, const char *s, void *p1, void *p2,
				       void *p3)
{
	char *p = p1;
	size_t *size = p2;

	if (idx == 0 && count == 1) {
		if (strlen(s) >= *size) {
			return -ENOBUFS;
		}

		strcpy(p, s);

		return 0;
	}

	return -EINVAL;
}

int ctr_lte_talk_at_xsocket(int p1, int *p2, int *p3, char *buf, size_t size)
{
	int ret;

	if (p2 == NULL && p3 == NULL) {
		ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_xsocket_response_handler, buf,
					   &size, NULL, "AT#XSOCKET=%d", p1);

	} else if (p2 != NULL && p3 != NULL) {
		ret = talk_cmd_response_ok(RESPONSE_TIMEOUT_S, at_xsocket_response_handler, buf,
					   &size, NULL, "AT#XSOCKET=%d,%d,%d", p1, *p2, *p3);

	} else {
		return -EINVAL;
	}

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_response_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xsystemmode(int p1, int p2, int p3, int p4)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XSYSTEMMODE=%d,%d,%d,%d", p1, p2, p3, p4);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_at_xtime(int p1)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT%%XTIME=%d", p1);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}
