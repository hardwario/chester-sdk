#include "ctr_lrw_talk.h"

#include <chester/ctr_util.h>
#include <chester/drivers/ctr_lrw_if.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lrw_talk, CONFIG_CTR_LRW_LOG_LEVEL);

#define SEND_GUARD_TIME	   K_MSEC(50)
#define BAND_CHANGE_TIME   K_MSEC(5000)
#define RESPONSE_TIMEOUT_S K_MSEC(1000)
#define RESPONSE_TIMEOUT_L K_SECONDS(10)

typedef bool (*handler_cb)(const char *s, void *param);

static const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(ctr_lrw_if));

static handler_cb m_handler_cb;
static ctr_lrw_talk_event_cb m_event_cb;
static K_MUTEX_DEFINE(m_handler_mut);
static K_MUTEX_DEFINE(m_talk_mut);
static struct k_poll_signal m_response_sig;
static void *m_handler_param;

static bool m_received_recv_preamble;
static char m_recv_buf[128];
static int m_recv_port;
static int m_recv_len;

static int process_recv_payload(struct ctr_lrw_talk_event *event, const char *hexbuf, int recv_port,
				int recv_len)
{
	int len = strlen(hexbuf);
	if (len == 0 || recv_len == 0) {
		return -ENOMSG;
	}

	if (len > 0) {
		char cb[3];

		for (int i = 0; i < len / 2; i++) {
			cb[0] = hexbuf[2 * i];
			cb[1] = hexbuf[2 * i + 1];

			m_recv_buf[i] = strtol(cb, NULL, 16);
		}

		m_recv_buf[len / 2] = '\0';

		LOG_INF("RECV: port: %d len: %d msg: %s", recv_port, recv_len, m_recv_buf);

		if (recv_len == strlen(m_recv_buf)) {
			if (m_event_cb != NULL) {
				event->data.recv_data.port = recv_port;
				event->data.recv_data.len = recv_len;
				event->data.recv_data.buf = k_calloc(recv_len + 1, sizeof(char));
				if (event->data.recv_data.buf != NULL) {
					strcpy(event->data.recv_data.buf, m_recv_buf);

					event->type = CTR_LRW_TALK_EVENT_RECV;

					return 0;

				} else {
					LOG_ERR("No memory to receive message");
					return -ENOMEM;
				}
			}
		}
		LOG_ERR("RECV message has inconsistent length: got %d, expected %d",
			strlen(m_recv_buf), recv_len);
		return -EINVAL;
	}

	LOG_INF("No RECV message");
	return ENOMSG;
}

static void process_urc(const char *s)
{
	struct ctr_lrw_talk_event event = {0};

	LOG_DBG("%s", s);
	if (strcmp(s, "+ACK") == 0) {
		if (m_event_cb != NULL) {
			event.type = CTR_LRW_TALK_EVENT_SEND_OK;
			m_event_cb(event);
		}

		return;
	}

	if (strcmp(s, "+NOACK") == 0) {
		if (m_event_cb != NULL) {
			event.type = CTR_LRW_TALK_EVENT_SEND_ERR;
			m_event_cb(event);
		}

		return;
	}

	if (strcmp(s, "+EVENT=1,1") == 0) {
		if (m_event_cb != NULL) {
			event.type = CTR_LRW_TALK_EVENT_JOIN_OK;
			m_event_cb(event);
		}

		return;
	}

	if (strcmp(s, "+EVENT=1,0") == 0) {
		if (m_event_cb != NULL) {
			event.type = CTR_LRW_TALK_EVENT_JOIN_ERR;
			m_event_cb(event);
		}

		return;
	}

	if (strcmp(s, "+EVENT=0,0") == 0) {
		if (m_event_cb != NULL) {
			event.type = CTR_LRW_TALK_EVENT_BOOT;
			m_event_cb(event);
		}

		return;
	}

	if (strncmp(s, "$RECVGET=", 9) == 0) {
		int port = atoi(strchr(s, (int)'=') + 1);
		const char *msg_cnt_start = strchr(s, (int)',') + 1;
		int msg_cnt = atoi(msg_cnt_start);
		LOG_DBG("FIFO has %d messages", msg_cnt);
		const char *len_start = strchr(msg_cnt_start, (int)',') + 1;
		int len = atoi(len_start);
		const char *msg_start = strchr(len_start, (int)',') + 1;

		if (process_recv_payload(&event, msg_start, port, len) == 0 && m_event_cb != NULL) {
			event.type = CTR_LRW_TALK_EVENT_RECV;
			m_event_cb(event);
		}

		return;
	}

	if (m_received_recv_preamble) {
		m_received_recv_preamble = false;

		if (process_recv_payload(&event, s, m_recv_port, m_recv_len) == 0 &&
		    m_event_cb != NULL) {

			event.type = CTR_LRW_TALK_EVENT_RECV;
			m_event_cb(event);
		}

		m_recv_port = 0;
		m_recv_len = 0;
		m_received_recv_preamble = false;
	}

	if (strncmp(s, "+RECV=", 6) == 0) {
		m_received_recv_preamble = true;

		m_recv_port = atoi(strchr(s, (int)'=') + 1);
		m_recv_len = atoi(strchr(s, (int)',') + 1);
	}
}

static void recv_cb(const char *s)
{
	k_mutex_lock(&m_handler_mut, K_FOREVER);

	if (m_handler_cb != NULL && m_handler_cb(s, m_handler_param)) {
		k_mutex_unlock(&m_handler_mut);
		LOG_INF("RSP: %s", s);
		k_poll_signal_raise(&m_response_sig, 0);

	} else {
		k_mutex_unlock(&m_handler_mut);
		LOG_INF("URC: %s", s);
		process_urc(s);
	}
}

static int wait_response(k_timeout_t timeout, handler_cb cb, void *param)
{
	int ret;

	k_poll_signal_reset(&m_response_sig);

	k_mutex_lock(&m_handler_mut, K_FOREVER);
	m_handler_cb = cb;
	m_handler_param = param;
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

	return 0;
}

static bool ok_handler(const char *s, void *param)
{
	if (strcmp(s, "+OK") != 0) {
		return false;
	}

	return true;
}

static int talk_cmd(const char *fmt, ...)
{
	int ret;

	/* TODO Do not block long operation by mutex but return -EBUSY instead */
	k_mutex_lock(&m_talk_mut, K_FOREVER);
	k_sleep(SEND_GUARD_TIME);

	va_list ap;

	va_start(ap, fmt);
	ret = ctr_lrw_if_send(dev, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_if_send` failed: %d", ret);
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
	ret = ctr_lrw_if_send(dev, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_if_send` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	ret = wait_response(timeout, ok_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `wait_response` failed: %d", ret);
		k_mutex_unlock(&m_talk_mut);
		return ret;
	}

	k_mutex_unlock(&m_talk_mut);
	return 0;
}

int ctr_lrw_talk_init(ctr_lrw_talk_event_cb event_cb)
{
	int ret;

	m_event_cb = event_cb;

	k_poll_signal_init(&m_response_sig);

	ret = ctr_lrw_if_init(dev, recv_cb);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_if_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_enable(void)
{
	int ret;

	ret = ctr_lrw_if_enable(dev);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_if_enable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_disable(void)
{
	int ret;

	ret = ctr_lrw_if_disable(dev);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_if_disable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_(const char *s)
{
	int ret;

	ret = talk_cmd("%s", s);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at(void)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_dformat(uint8_t df)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DFORMAT=%u", df);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_band(uint8_t band)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+BAND=%u", band);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	k_sleep(BAND_CHANGE_TIME);

	return 0;
}

int ctr_lrw_talk_at_class(uint8_t class)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CLASS=%u", class);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_dr(uint8_t dr)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DR=%u", dr);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_mode(uint8_t mode)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+MODE=%u", mode);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_nwk(uint8_t network)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+NWK=%u", network);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_adr(uint8_t adr)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+ADR=%u", adr);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_dutycycle(uint8_t dc)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DUTYCYCLE=%u", dc);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_joindc(uint8_t jdc)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+JOINDC=%u", jdc);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_join(void)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+JOIN");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_devaddr(const uint8_t *devaddr, size_t devaddr_size)
{
	int ret;

	size_t hex_size = devaddr_size * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(devaddr, devaddr_size, hex, hex_size, true);

	if (ret != devaddr_size * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DEVADDR=%s", hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_deveui(const uint8_t *deveui, size_t deveui_size)
{
	int ret;

	size_t hex_size = deveui_size * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(deveui, deveui_size, hex, hex_size, true);

	if (ret != deveui_size * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+DEVEUI=%s", hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_appeui(const uint8_t *appeui, size_t appeui_size)
{
	int ret;

	size_t hex_size = appeui_size * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(appeui, appeui_size, hex, hex_size, true);

	if (ret != appeui_size * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+APPEUI=%s", hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_appkey(const uint8_t *appkey, size_t appkey_size)
{
	int ret;

	size_t hex_size = appkey_size * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(appkey, appkey_size, hex, hex_size, true);

	if (ret != appkey_size * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+APPKEY=%s", hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_nwkskey(const uint8_t *nwkskey, size_t nwkskey_size)
{
	int ret;

	size_t hex_size = nwkskey_size * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(nwkskey, nwkskey_size, hex, hex_size, true);

	if (ret != nwkskey_size * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+NWKSKEY=%s", hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_appskey(const uint8_t *appskey, size_t appskey_size)
{
	int ret;

	size_t hex_size = appskey_size * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(appskey, appskey_size, hex, hex_size, true);

	if (ret != appskey_size * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+APPSKEY=%s", hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_delays(const uint32_t *delays)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+DELAY=%d,%d,%d,%d", delays[0], delays[1],
			  delays[2], delays[3]);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_recv_get(void)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT$RECVGET?");

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_recvurc(uint8_t enable)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT$RECVURC=%d", enable);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_chmask(const char *chmask)
{
	int ret;

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_S, "AT+CHMASK=%s", chmask);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_talk_at_utx(const void *payload, size_t payload_len)
{
	int ret;

	if (payload_len < 1 || payload_len > 242) {
		return -EINVAL;
	}

	size_t hex_size = payload_len * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);

	if (ret != payload_len * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+UTX %u\r%s", payload_len, hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_ctx(const void *payload, size_t payload_len)
{
	int ret;

	if (payload_len < 1 || payload_len > 242) {
		return -EINVAL;
	}

	size_t hex_size = payload_len * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);

	if (ret != payload_len * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+CTX %u\r%s", payload_len, hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_putx(uint8_t port, const void *payload, size_t payload_len)
{
	int ret;

	if (payload_len < 1 || payload_len > 242) {
		return -EINVAL;
	}

	size_t hex_size = payload_len * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);

	if (ret != payload_len * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+PUTX %u,%u\r%s", port, payload_len, hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}

int ctr_lrw_talk_at_pctx(uint8_t port, const void *payload, size_t payload_len)
{
	int ret;

	if (payload_len < 1 || payload_len > 242) {
		return -EINVAL;
	}

	size_t hex_size = payload_len * 2 + 1;
	char *hex = k_malloc(hex_size);

	if (hex == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	ret = ctr_buf2hex(payload, payload_len, hex, hex_size, true);

	if (ret != payload_len * 2) {
		LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	ret = talk_cmd_ok(RESPONSE_TIMEOUT_L, "AT+PCTX %u,%u\r%s", port, payload_len, hex);

	if (ret < 0) {
		LOG_ERR("Call `talk_cmd_ok` failed: %d", ret);
		k_free(hex);
		return ret;
	}

	k_free(hex);
	return 0;
}
