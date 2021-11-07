#include <ctr_lte_talk.h>
#include <ctr_lte_uart.h>
#include <ctr_util.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte_talk, LOG_LEVEL_DBG);

#define SEND_GUARD_TIME K_MSEC(100)
#define RESPONSE_TIMEOUT_S K_MSEC(1000)
#define RESPONSE_TIMEOUT_L K_SECONDS(10)

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

static handler_cb m_handler_cb;
static ctr_lte_talk_event_cb m_event_cb;
static K_MUTEX_DEFINE(m_handler_mut);
static K_MUTEX_DEFINE(m_talk_mut);
static struct k_poll_signal m_response_sig;
static void *m_handler_param;

static void process_urc(const char *s)
{
	if (strlen(s) != 0) {
		LOG_INF("URC: %s", s);
	}

	if (strcmp(s, "Ready") == 0) {
		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_TALK_EVENT_BOOT);
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

static int talk_cmd_ok(k_timeout_t timeout, const char *fmt, ...)
{
	int ret;

	/* TODO Do not block long operation by mutex but return -EBUSY instead */
	k_mutex_lock(&m_talk_mut, K_FOREVER);
	k_sleep(SEND_GUARD_TIME);

	va_list ap;

	va_start(ap, fmt);
	ret = ctr_lte_uart_send(fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_uart_send` failed: %d", ret);
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
	ret = ctr_lte_uart_send(fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_uart_send` failed: %d", ret);
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

int ctr_lte_talk_init(ctr_lte_talk_event_cb event_cb)
{
	int ret;

	m_event_cb = event_cb;

	k_poll_signal_init(&m_response_sig);

	ret = ctr_lte_uart_init(recv_cb);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_uart_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_enable(void)
{
	int ret;

	ret = ctr_lte_uart_enable();

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_uart_enable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_talk_disable(void)
{
	int ret;

	ret = ctr_lte_uart_disable();

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_uart_disable` failed: %d", ret);
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

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CFUN=%d", p1);

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
	ARG_UNUSED(p3);

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
	ARG_UNUSED(p3);

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
