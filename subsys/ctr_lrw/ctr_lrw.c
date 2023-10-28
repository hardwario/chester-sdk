/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* TODO Implement retries settings parameter */

#include "ctr_lrw_talk.h"

#include <chester/ctr_config.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_util.h>
#include <chester/drivers/ctr_lrw_if.h>
#include <chester/drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lrw, CONFIG_CTR_LRW_LOG_LEVEL);

#define SETTINGS_PFX "lrw"

#define BOOT_RETRY_COUNT     3
#define BOOT_RETRY_DELAY     K_SECONDS(10)
#define SETUP_RETRY_COUNT    3
#define SETUP_RETRY_DELAY    K_SECONDS(10)
#define JOIN_TIMEOUT	     K_SECONDS(120)
#define JOIN_RETRY_COUNT     3
#define JOIN_RETRY_DELAY     K_SECONDS(30)
#define SEND_TIMEOUT	     K_SECONDS(120)
#define SEND_RETRY_COUNT     3
#define SEND_RETRY_DELAY     K_SECONDS(30)
#define REQ_INTRA_DELAY_MSEC 8000

#define CMD_MSGQ_MAX_ITEMS  16
#define SEND_MSGQ_MAX_ITEMS 16

enum cmd_msgq_req {
	CMD_MSGQ_REQ_START = 0,
	CMD_MSGQ_REQ_JOIN = 1,
};

struct cmd_msgq_item {
	int corr_id;
	enum cmd_msgq_req req;
};

struct send_msgq_data {
	int64_t ttl;
	bool confirmed;
	int port;
	void *buf;
	size_t len;
};

struct send_msgq_item {
	int corr_id;
	struct send_msgq_data data;
};

enum state {
	STATE_ERROR = -1,
	STATE_INIT = 0,
	STATE_READY = 1,
};

enum antenna {
	ANTENNA_INT = 0,
	ANTENNA_EXT = 1,
};

enum band {
	BAND_AS923 = 0,
	BAND_AU915 = 1,
	BAND_EU868 = 5,
	BAND_KR920 = 6,
	BAND_IN865 = 7,
	BAND_US915 = 8,
};

enum class {
	CLASS_A = 0,
	CLASS_C = 2,
};

enum mode {
	MODE_ABP = 0,
	MODE_OTAA = 1,
};

enum nwk {
	NWK_PRIVATE = 0,
	NWK_PUBLIC = 1,
};

struct config {
	bool test;
	enum antenna antenna;
	enum band band;
	char chmask[24 + 1];
	enum class class;
	enum mode mode;
	enum nwk nwk;
	bool adr;
	int datarate;
	bool dutycycle;
	uint8_t devaddr[4];
	uint8_t deveui[8];
	uint8_t joineui[8];
	uint8_t appkey[16];
	uint8_t nwkskey[16];
	uint8_t appskey[16];
};

static const struct device *dev_lrw_if = DEVICE_DT_GET(DT_CHOSEN(ctr_lrw_if));
static const struct device *dev_rfmux = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

static enum state m_state = STATE_INIT;

static struct config m_config_interim = {
	.antenna = ANTENNA_INT,
	.band = BAND_EU868,
	.class = CLASS_A,
	.mode = MODE_OTAA,
	.nwk = NWK_PUBLIC,
	.adr = true,
	.datarate = 0,
	.dutycycle = true,
};

static struct config m_config;

static struct k_poll_signal m_boot_sig;
static struct k_poll_signal m_join_sig;
static struct k_poll_signal m_send_sig;

K_MSGQ_DEFINE(m_cmd_msgq, sizeof(struct cmd_msgq_item), CMD_MSGQ_MAX_ITEMS, 4);
K_MSGQ_DEFINE(m_send_msgq, sizeof(struct send_msgq_item), SEND_MSGQ_MAX_ITEMS, 4);

static ctr_lrw_event_cb m_callback;
static void *m_param;
static atomic_t m_corr_id;
static int64_t m_last_req;

static void talk_handler(enum ctr_lrw_talk_event event)
{
	switch (event) {
	case CTR_LRW_TALK_EVENT_BOOT:
		LOG_DBG("Event `CTR_LRW_TALK_EVENT_BOOT`");
		k_poll_signal_raise(&m_boot_sig, 0);
		break;
	case CTR_LRW_TALK_EVENT_JOIN_OK:
		LOG_DBG("Event `CTR_LRW_TALK_EVENT_JOIN_OK`");
		k_poll_signal_raise(&m_join_sig, 0);
		break;
	case CTR_LRW_TALK_EVENT_JOIN_ERR:
		LOG_DBG("Event `CTR_LRW_TALK_EVENT_JOIN_ERR`");
		k_poll_signal_raise(&m_join_sig, 1);
		break;
	case CTR_LRW_TALK_EVENT_SEND_OK:
		LOG_DBG("Event `CTR_LRW_TALK_EVENT_SEND_OK`");
		k_poll_signal_raise(&m_send_sig, 0);
		break;
	case CTR_LRW_TALK_EVENT_SEND_ERR:
		LOG_DBG("Event `CTR_LRW_TALK_EVENT_SEND_ERR`");
		k_poll_signal_raise(&m_send_sig, 1);
		break;
	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

static int boot_once(void)
{
	int ret;

	/* TODO Address critical timing between m_boot_sig and reset? */
	k_poll_signal_reset(&m_boot_sig);

	ret = ctr_lrw_if_reset(dev_lrw_if);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_if_reset` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_boot_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(1000));

	if (ret == -EAGAIN) {
		LOG_WRN("Boot message timed out");
		return -ETIMEDOUT;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int boot(int retries, k_timeout_t delay)
{
	int ret;

	LOG_INF("Operation BOOT started");

	while (retries-- > 0) {
		ret = ctr_lrw_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = boot_once();

		if (ret == 0) {
			ret = ctr_lrw_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
				return ret;
			}

			LOG_INF("Operation BOOT succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `boot_once` failed: %d", ret);
		}

		ret = ctr_lrw_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating BOOT operation (retries left: %d)", retries);
		}
	}

	LOG_ERR("Operation BOOT failed");
	return -ENOLINK;
}

static int setup_once(void)
{
	int ret;

	ret = ctr_lrw_talk_at_dformat(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_dformat` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_band((uint8_t)m_config.band);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_band` failed: %d", ret);
		return ret;
	}

	if (strlen(m_config.chmask)) {
		char chmask[sizeof(m_config.chmask)];

		strcpy(chmask, m_config.chmask);

		size_t len = strlen(chmask);

		for (size_t i = 0; i < len; i++) {
			chmask[i] = toupper(chmask[i]);
		}

		ret = ctr_lrw_talk_at_chmask(chmask);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_chmask` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_lrw_talk_at_class((uint8_t)m_config.class);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_class` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_mode((uint8_t)m_config.mode);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_mode` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_nwk((uint8_t)m_config.nwk);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_nwk` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_adr(m_config.adr ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_adr` failed: %d", ret);
		return ret;
	}

	if (!m_config.adr) {
		ret = ctr_lrw_talk_at_dr(m_config.datarate);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_dr` failed: %d", ret);
			return ret;
		}
	}

	if (m_config.band == BAND_EU868) {
		ret = ctr_lrw_talk_at_dutycycle(m_config.dutycycle ? 1 : 0);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_dutycycle` failed: %d", ret);
			return ret;
		}
	}

	if (m_config.mode != MODE_ABP && m_config.band != BAND_AU915 &&
	    m_config.band != BAND_US915) {
		ret = ctr_lrw_talk_at_joindc(m_config.dutycycle ? 1 : 0);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_joindc` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_lrw_talk_at_devaddr(m_config.devaddr, sizeof(m_config.devaddr));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_devaddr` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_deveui(m_config.deveui, sizeof(m_config.deveui));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_deveui` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_appeui(m_config.joineui, sizeof(m_config.joineui));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_appeui` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_appkey(m_config.appkey, sizeof(m_config.appkey));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_appkey` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_nwkskey(m_config.nwkskey, sizeof(m_config.nwkskey));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_nwkskey` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_talk_at_appskey(m_config.appskey, sizeof(m_config.appskey));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_appskey` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int setup(int retries, k_timeout_t delay)
{
	int ret;

	LOG_INF("Operation SETUP started");

	while (retries-- > 0) {
		ret = ctr_lrw_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = setup_once();

		if (ret == 0) {
			ret = ctr_lrw_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
				return ret;
			}

			LOG_INF("Operation SETUP succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `setup_once` failed: %d", ret);
		}

		ret = ctr_lrw_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating SETUP operation (retries left: %d)", retries);
		}
	}

	LOG_ERR("Operation SETUP failed");
	return -EPIPE;
}

static int join_once(void)
{
	int ret;

	k_poll_signal_reset(&m_join_sig);

	ret = ctr_lrw_talk_at_join();

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_talk_at_join` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_join_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), JOIN_TIMEOUT);

	if (ret == -EAGAIN) {
		LOG_WRN("Join response timed out");
		return -ETIMEDOUT;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	unsigned int signaled;
	int result;

	k_poll_signal_check(&m_join_sig, &signaled, &result);

	if (signaled == 0 || result != 0) {
		return -ENOTCONN;
	}

	return 0;
}

static int join(int retries, k_timeout_t delay)
{
	int ret;

	LOG_INF("Operation JOIN started");

	while (retries-- > 0) {
		ret = ctr_lrw_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = join_once();

		if (ret == 0) {
			ret = ctr_lrw_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
				return ret;
			}

			LOG_INF("Operation JOIN succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `join_once` failed: %d", ret);
		}

		ret = ctr_lrw_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating JOIN operation (retries left: %d)", retries);
		}
	}

	LOG_WRN("Operation JOIN failed");
	return -ENOTCONN;
}

static int send_once(const struct send_msgq_data *data)
{
	int ret;

	if (!data->confirmed && data->port < 0) {
		ret = ctr_lrw_talk_at_utx(data->buf, data->len);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_utx` failed: %d", ret);
			return ret;
		}

		k_poll_signal_raise(&m_send_sig, 0);

	} else if (data->confirmed && data->port < 0) {
		ret = ctr_lrw_talk_at_ctx(data->buf, data->len);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_ctx` failed: %d", ret);
			return ret;
		}

		k_poll_signal_reset(&m_send_sig);

	} else if (!data->confirmed && data->port >= 0) {
		ret = ctr_lrw_talk_at_putx(data->port, data->buf, data->len);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_putx` failed: %d", ret);
			return ret;
		}

		k_poll_signal_raise(&m_send_sig, 0);

	} else {
		ret = ctr_lrw_talk_at_pctx(data->port, data->buf, data->len);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_at_pctx` failed: %d", ret);
			return ret;
		}

		k_poll_signal_reset(&m_send_sig);
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_send_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), SEND_TIMEOUT);

	if (ret == -EAGAIN) {
		LOG_WRN("Send response timed out");

		/* Return positive error code intentionally */
		return ETIMEDOUT;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	unsigned int signaled;
	int result;

	k_poll_signal_check(&m_send_sig, &signaled, &result);

	if (signaled == 0 || result != 0) {
		/* Return positive error code intentionally */
		return ENOMSG;
	}

	return 0;
}

static int send(int retries, k_timeout_t delay, const struct send_msgq_data *data)
{
	int ret;
	int err = 0;

	LOG_INF("Operation SEND started");

	while (retries-- > 0) {
		ret = ctr_lrw_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = send_once(data);
		err = ret;

		if (ret == 0) {
			ret = ctr_lrw_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
				return ret;
			}

			LOG_INF("Operation SEND succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `send_once` failed: %d", ret);
		}

		ret = ctr_lrw_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating SEND operation (retries left: %d)", retries);
		}
	}

	LOG_WRN("Operation SEND failed");
	return err;
}

static int start(void)
{
	int ret;

	static bool initialized;

	if (!initialized) {
		ret = ctr_rfmux_acquire(dev_rfmux);
		if (ret) {
			LOG_ERR("Call `ctr_rfmux_acquire` failed: %d", ret);
			return ret;
		}

		ret = ctr_rfmux_set_antenna(dev_rfmux, m_config.antenna == ANTENNA_EXT
							       ? CTR_RFMUX_ANTENNA_EXT
							       : CTR_RFMUX_ANTENNA_INT);

		if (ret < 0) {
			LOG_ERR("Call `ctr_rfmux_set_antenna` failed: %d", ret);
			return ret;
		}

		ret = ctr_rfmux_set_interface(dev_rfmux, CTR_RFMUX_INTERFACE_LRW);

		if (ret < 0) {
			LOG_ERR("Call `ctr_rfmux_set_interface` failed: %d", ret);
			return ret;
		}

		initialized = true;
	}

	ret = boot(BOOT_RETRY_COUNT, BOOT_RETRY_DELAY);

	if (ret < 0) {
		LOG_ERR("Call `boot` failed: %d", ret);
		return ret;
	}

	ret = setup(SETUP_RETRY_COUNT, SETUP_RETRY_DELAY);

	if (ret < 0) {
		LOG_ERR("Call `setup` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int process_req_start(const struct cmd_msgq_item *item)
{
	int ret;

	union ctr_lrw_event_data data = {0};

	ret = start();

	if (ret < 0) {
		LOG_ERR("Call `start` failed: %d", ret);

		data.start_err.corr_id = item->corr_id;

		if (m_callback != NULL) {
			m_callback(CTR_LRW_EVENT_START_ERR, &data, m_param);
		}

		return ret;
	}

	m_state = STATE_READY;

	data.start_ok.corr_id = item->corr_id;

	if (m_callback != NULL) {
		m_callback(CTR_LRW_EVENT_START_OK, &data, m_param);
	}

	return 0;
}

static int process_req_join(const struct cmd_msgq_item *item)
{
	int ret;

	union ctr_lrw_event_data data = {0};

	ret = join(JOIN_RETRY_COUNT, JOIN_RETRY_DELAY);

	if (ret < 0) {
		LOG_ERR("Call `join` failed: %d", ret);

		data.join_err.corr_id = item->corr_id;

		if (m_callback != NULL) {
			m_callback(CTR_LRW_EVENT_JOIN_ERR, &data, m_param);
		}

		return ret;
	}

	data.join_ok.corr_id = item->corr_id;

	if (m_callback != NULL) {
		m_callback(CTR_LRW_EVENT_JOIN_OK, &data, m_param);
	}

	return 0;
}

static int process_req_send(const struct send_msgq_item *item)
{
	int ret;

	union ctr_lrw_event_data data = {0};

	if (item->data.ttl != 0) {
		if (k_uptime_get() > item->data.ttl) {
			LOG_WRN("Message TTL expired");

			k_free(item->data.buf);

			/* Return positive error code intentionally */
			return ECANCELED;
		}
	}

	ret = send(SEND_RETRY_COUNT, SEND_RETRY_DELAY, &item->data);

	k_free(item->data.buf);

	if (ret != 0) {
		LOG_ERR("Call `send` failed: %d", ret);

		data.send_err.corr_id = item->corr_id;

		if (m_callback != NULL) {
			m_callback(CTR_LRW_EVENT_SEND_ERR, &data, m_param);
		}

		return ret;
	}

	data.send_ok.corr_id = item->corr_id;

	if (m_callback != NULL) {
		m_callback(CTR_LRW_EVENT_SEND_OK, &data, m_param);
	}

	return 0;
}

static void wait_intra_req_delay(void)
{
	int64_t delta = k_uptime_get() - m_last_req;

	if (delta < REQ_INTRA_DELAY_MSEC) {
		delta = REQ_INTRA_DELAY_MSEC - delta;

		LOG_INF("Waiting %lld millisecond(s)", delta);

		while ((delta = k_msleep(delta))) {
			continue;
		}
	}
}

static void process_cmd_msgq(void)
{
	int ret;

	struct cmd_msgq_item item;

	ret = k_msgq_get(&m_cmd_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_get` failed: %d", ret);
		return;
	}

	wait_intra_req_delay();

	if (item.req == CMD_MSGQ_REQ_START) {
		LOG_INF("Dequeued START command (correlation id: %d)", item.corr_id);

		if (m_state != STATE_INIT && m_state != STATE_ERROR) {
			LOG_WRN("No reason for START operation - ignoring");

		} else {
			ret = process_req_start(&item);

			m_state = ret < 0 ? STATE_ERROR : STATE_READY;

			if (m_state == STATE_ERROR) {
				LOG_ERR("Error - flushing all control commands from queue");

				k_msgq_purge(&m_cmd_msgq);

				if (m_callback != NULL) {
					m_callback(CTR_LRW_EVENT_FAILURE, NULL, m_param);
				}
			}
		}

	} else if (item.req == CMD_MSGQ_REQ_JOIN) {
		LOG_INF("Dequeued JOIN command (correlation id: %d)", item.corr_id);

		if (m_state != STATE_READY) {
			LOG_WRN("Not ready for JOIN command - ignoring");

		} else {
			ret = process_req_join(&item);

			m_state = ret < 0 ? STATE_ERROR : STATE_READY;

			if (m_state == STATE_ERROR) {
				LOG_ERR("Error - flushing all control commands from queue");

				k_msgq_purge(&m_cmd_msgq);

				if (m_callback != NULL) {
					m_callback(CTR_LRW_EVENT_FAILURE, NULL, m_param);
				}
			}
		}

	} else {
		LOG_ERR("Unknown message: %d", (int)item.req);
	}

	m_last_req = k_uptime_get();
}

static void process_send_msgq(void)
{
	int ret;

	struct send_msgq_item item;

	ret = k_msgq_get(&m_send_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_get` failed: %d", ret);
		return;
	}

	wait_intra_req_delay();

	LOG_INF("Dequeued SEND command (correlation id: %d)", item.corr_id);

	ret = process_req_send(&item);

	m_state = ret < 0 ? STATE_ERROR : STATE_READY;

	if (m_state == STATE_ERROR) {
		LOG_ERR("Error - flushing all control commands from queue");

		k_msgq_purge(&m_cmd_msgq);

		if (m_callback != NULL) {
			m_callback(CTR_LRW_EVENT_FAILURE, NULL, m_param);
		}
	}

	m_last_req = k_uptime_get();
}

static void dispatcher_thread(void)
{
	int ret;

	for (;;) {
		struct k_poll_event events[] = {
			K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
						 K_POLL_MODE_NOTIFY_ONLY, &m_cmd_msgq),
			K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
						 K_POLL_MODE_NOTIFY_ONLY, &m_send_msgq),
		};

		ret = k_poll(events, m_state != STATE_READY ? 1 : 2, K_FOREVER);

		if (ret < 0) {
			LOG_ERR("Call `k_poll` failed: %d", ret);
			k_sleep(K_SECONDS(5));
			continue;
		}

		if (events[0].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
			process_cmd_msgq();
		}

		if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE) {
			if (m_state == STATE_READY) {
				process_send_msgq();
			}
		}
	}
}

K_THREAD_DEFINE(ctr_lrw, CONFIG_CTR_LRW_THREAD_STACK_SIZE, dispatcher_thread, NULL, NULL, NULL,
		CONFIG_CTR_LRW_THREAD_PRIORITY, 0, 0);

int ctr_lrw_init(ctr_lrw_event_cb callback, void *param)
{
	m_callback = callback;
	m_param = param;

	return 0;
}

int ctr_lrw_start(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_START,
	};

	LOG_INF("Enqueing START command (correlation id: %d)", item.corr_id);

	if (corr_id != NULL) {
		*corr_id = item.corr_id;
	}

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_join(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	if (m_config.mode != MODE_OTAA) {
		LOG_WRN("Not in OTAA mode - ignoring");
		return 0;
	}

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_JOIN,
	};

	if (corr_id != NULL) {
		*corr_id = item.corr_id;
	}

	LOG_INF("Enqueing JOIN command (correlation id: %d)", item.corr_id);

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lrw_send(const struct ctr_lrw_send_opts *opts, const void *buf, size_t len, int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	void *p = k_malloc(len);

	if (p == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	memcpy(p, buf, len);

	struct send_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.data = {.ttl = opts->ttl,
			 .confirmed = opts->confirmed,
			 .port = opts->port,
			 .buf = p,
			 .len = len},
	};

	LOG_INF("Enqueing SEND command (correlation id: %d)", item.corr_id);

	if (corr_id != NULL) {
		*corr_id = item.corr_id;
	}

	ret = k_msgq_put(&m_send_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		k_free(p);
		return ret;
	}

	return 0;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET(_key, _var, _size)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != _size) {                                                        \
				return -EINVAL;                                                    \
			}                                                                          \
                                                                                                   \
			ret = read_cb(cb_arg, _var, len);                                          \
                                                                                                   \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
                                                                                                   \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET("test", &m_config_interim.test, sizeof(m_config_interim.test));
	SETTINGS_SET("antenna", &m_config_interim.antenna, sizeof(m_config_interim.antenna));
	SETTINGS_SET("band", &m_config_interim.band, sizeof(m_config_interim.band));
	SETTINGS_SET("chmask", m_config_interim.chmask, sizeof(m_config_interim.chmask));
	SETTINGS_SET("class", &m_config_interim.class, sizeof(m_config_interim.class));
	SETTINGS_SET("mode", &m_config_interim.mode, sizeof(m_config_interim.mode));
	SETTINGS_SET("nwk", &m_config_interim.nwk, sizeof(m_config_interim.nwk));
	SETTINGS_SET("adr", &m_config_interim.adr, sizeof(m_config_interim.adr));
	SETTINGS_SET("datarate", &m_config_interim.datarate, sizeof(m_config_interim.datarate));
	SETTINGS_SET("dutycycle", &m_config_interim.dutycycle, sizeof(m_config_interim.dutycycle));
	SETTINGS_SET("devaddr", m_config_interim.devaddr, sizeof(m_config_interim.devaddr));
	SETTINGS_SET("deveui", m_config_interim.deveui, sizeof(m_config_interim.deveui));
	SETTINGS_SET("joineui", m_config_interim.joineui, sizeof(m_config_interim.joineui));
	SETTINGS_SET("appkey", m_config_interim.appkey, sizeof(m_config_interim.appkey));
	SETTINGS_SET("nwkskey", m_config_interim.nwkskey, sizeof(m_config_interim.nwkskey));
	SETTINGS_SET("appskey", m_config_interim.appskey, sizeof(m_config_interim.appskey));

#undef SETTINGS_SET

	return -ENOENT;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&m_config, &m_config_interim, sizeof(m_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
#define EXPORT_FUNC(_key, _var, _size)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, _size);                             \
	} while (0)

	EXPORT_FUNC("test", &m_config_interim.test, sizeof(m_config_interim.test));
	EXPORT_FUNC("antenna", &m_config_interim.antenna, sizeof(m_config_interim.antenna));
	EXPORT_FUNC("band", &m_config_interim.band, sizeof(m_config_interim.band));
	EXPORT_FUNC("chmask", m_config_interim.chmask, sizeof(m_config_interim.chmask));
	EXPORT_FUNC("class", &m_config_interim.class, sizeof(m_config_interim.class));
	EXPORT_FUNC("mode", &m_config_interim.mode, sizeof(m_config_interim.mode));
	EXPORT_FUNC("nwk", &m_config_interim.nwk, sizeof(m_config_interim.nwk));
	EXPORT_FUNC("adr", &m_config_interim.adr, sizeof(m_config_interim.adr));
	EXPORT_FUNC("datarate", &m_config_interim.datarate, sizeof(m_config_interim.datarate));
	EXPORT_FUNC("dutycycle", &m_config_interim.dutycycle, sizeof(m_config_interim.dutycycle));
	EXPORT_FUNC("devaddr", m_config_interim.devaddr, sizeof(m_config_interim.devaddr));
	EXPORT_FUNC("deveui", m_config_interim.deveui, sizeof(m_config_interim.deveui));
	EXPORT_FUNC("joineui", m_config_interim.joineui, sizeof(m_config_interim.joineui));
	EXPORT_FUNC("appkey", m_config_interim.appkey, sizeof(m_config_interim.appkey));
	EXPORT_FUNC("nwkskey", m_config_interim.nwkskey, sizeof(m_config_interim.nwkskey));
	EXPORT_FUNC("appskey", m_config_interim.appskey, sizeof(m_config_interim.appskey));

#undef EXPORT_FUNC

	return 0;
}

static void print_test(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config test %s",
		    m_config_interim.test ? "true" : "false");
}

static void print_antenna(const struct shell *shell)
{
	switch (m_config_interim.antenna) {
	case ANTENNA_INT:
		shell_print(shell, SETTINGS_PFX " config antenna int");
		break;
	case ANTENNA_EXT:
		shell_print(shell, SETTINGS_PFX " config antenna ext");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config antenna (unknown)");
	}
}

static void print_band(const struct shell *shell)
{
	switch (m_config_interim.band) {
	case BAND_AS923:
		shell_print(shell, SETTINGS_PFX " config band as923");
		break;
	case BAND_AU915:
		shell_print(shell, SETTINGS_PFX " config band au915");
		break;
	case BAND_EU868:
		shell_print(shell, SETTINGS_PFX " config band eu868");
		break;
	case BAND_KR920:
		shell_print(shell, SETTINGS_PFX " config band kr920");
		break;
	case BAND_IN865:
		shell_print(shell, SETTINGS_PFX " config band in865");
		break;
	case BAND_US915:
		shell_print(shell, SETTINGS_PFX " config band us915");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config band (unknown)");
	}
}

static void print_chmask(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config chmask %s", m_config_interim.chmask);
}

static void print_class(const struct shell *shell)
{
	switch (m_config_interim.class) {
	case CLASS_A:
		shell_print(shell, SETTINGS_PFX " config class a");
		break;
	case CLASS_C:
		shell_print(shell, SETTINGS_PFX " config class c");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config class (unknown)");
	}
}

static void print_mode(const struct shell *shell)
{
	switch (m_config_interim.mode) {
	case MODE_ABP:
		shell_print(shell, SETTINGS_PFX " config mode abp");
		break;
	case MODE_OTAA:
		shell_print(shell, SETTINGS_PFX " config mode otaa");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config mode (unknown)");
	}
}

static void print_nwk(const struct shell *shell)
{
	switch (m_config_interim.nwk) {
	case NWK_PRIVATE:
		shell_print(shell, SETTINGS_PFX " config nwk private");
		break;
	case NWK_PUBLIC:
		shell_print(shell, SETTINGS_PFX " config nwk public");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config nwk (unknown)");
	}
}

static void print_adr(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config adr %s", m_config_interim.adr ? "true" : "false");
}

static void print_datarate(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config datarate %d", m_config_interim.datarate);
}

static void print_dutycycle(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config dutycycle %s",
		    m_config_interim.dutycycle ? "true" : "false");
}

static void print_devaddr(const struct shell *shell)
{
	char buf[sizeof(m_config_interim.devaddr) * 2 + 1];

	if (ctr_buf2hex(m_config_interim.devaddr, sizeof(m_config_interim.devaddr), buf,
			sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config devaddr %s", buf);
	}
}

static void print_deveui(const struct shell *shell)
{
	char buf[sizeof(m_config_interim.deveui) * 2 + 1];

	if (ctr_buf2hex(m_config_interim.deveui, sizeof(m_config_interim.deveui), buf, sizeof(buf),
			false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config deveui %s", buf);
	}
}

static void print_joineui(const struct shell *shell)
{
	char buf[sizeof(m_config_interim.joineui) * 2 + 1];

	if (ctr_buf2hex(m_config_interim.joineui, sizeof(m_config_interim.joineui), buf,
			sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config joineui %s", buf);
	}
}

static void print_appkey(const struct shell *shell)
{
	char buf[sizeof(m_config_interim.appkey) * 2 + 1];

	if (ctr_buf2hex(m_config_interim.appkey, sizeof(m_config_interim.appkey), buf, sizeof(buf),
			false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config appkey %s", buf);
	}
}

static void print_nwkskey(const struct shell *shell)
{
	char buf[sizeof(m_config_interim.nwkskey) * 2 + 1];

	if (ctr_buf2hex(m_config_interim.nwkskey, sizeof(m_config_interim.nwkskey), buf,
			sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config nwkskey %s", buf);
	}
}

static void print_appskey(const struct shell *shell)
{
	char buf[sizeof(m_config_interim.appskey) * 2 + 1];

	if (ctr_buf2hex(m_config_interim.appskey, sizeof(m_config_interim.appskey), buf,
			sizeof(buf), false) >= 0) {
		shell_print(shell, SETTINGS_PFX " config appskey %s", buf);
	}
}

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_test(shell);
	print_antenna(shell);
	print_band(shell);
	print_chmask(shell);
	print_class(shell);
	print_mode(shell);
	print_nwk(shell);
	print_adr(shell);
	print_datarate(shell);
	print_dutycycle(shell);
	print_devaddr(shell);
	print_deveui(shell);
	print_joineui(shell);
	print_appkey(shell);
	print_nwkskey(shell);
	print_appskey(shell);

	return 0;
}

static int cmd_config_test(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_test(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.test = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.test = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_antenna(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_antenna(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "int") == 0) {
		m_config_interim.antenna = ANTENNA_INT;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "ext") == 0) {
		m_config_interim.antenna = ANTENNA_EXT;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_band(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_band(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "as923") == 0) {
		m_config_interim.band = BAND_AS923;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "au915") == 0) {
		m_config_interim.band = BAND_AU915;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "eu868") == 0) {
		m_config_interim.band = BAND_EU868;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "kr920") == 0) {
		m_config_interim.band = BAND_KR920;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "in865") == 0) {
		m_config_interim.band = BAND_IN865;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "us915") == 0) {
		m_config_interim.band = BAND_US915;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_chmask(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_chmask(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config_interim.chmask)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			char c = argv[1][i];

			if ((c < '0' || c > '9') && (c < 'a' || c > 'f')) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		strcpy(m_config_interim.chmask, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_class(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_class(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "a") == 0) {
		m_config_interim.class = CLASS_A;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "c") == 0) {
		m_config_interim.class = CLASS_C;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_mode(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "otaa") == 0) {
		m_config_interim.mode = MODE_OTAA;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "abp") == 0) {
		m_config_interim.mode = MODE_ABP;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_nwk(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_nwk(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "private") == 0) {
		m_config_interim.nwk = NWK_PRIVATE;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "public") == 0) {
		m_config_interim.nwk = NWK_PUBLIC;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_adr(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_adr(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.adr = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.adr = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_datarate(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_datarate(shell);
		return 0;
	}

	if (argc == 2) {
		if ((strlen(argv[1]) == 1 && isdigit((int)argv[1][0])) ||
		    (strlen(argv[1]) == 2 && isdigit((int)argv[1][0]) &&
		     isdigit((int)argv[1][1]))) {
			int datarate = atoi(argv[1]);

			if (datarate >= 0 && datarate <= 15) {
				m_config_interim.datarate = datarate;
				return 0;
			}
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_dutycycle(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_dutycycle(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.dutycycle = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.dutycycle = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_devaddr(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_devaddr(shell);
		return 0;
	}

	if (argc == 2) {
		int ret = ctr_hex2buf(argv[1], m_config_interim.devaddr,
				      sizeof(m_config_interim.devaddr), true);

		if (ret == sizeof(m_config_interim.devaddr)) {
			return 0;
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_deveui(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_deveui(shell);
		return 0;
	}

	if (argc == 2) {
		int ret = ctr_hex2buf(argv[1], m_config_interim.deveui,
				      sizeof(m_config_interim.deveui), true);

		if (ret == sizeof(m_config_interim.deveui)) {
			return 0;
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_joineui(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_joineui(shell);
		return 0;
	}

	if (argc == 2) {
		int ret = ctr_hex2buf(argv[1], m_config_interim.joineui,
				      sizeof(m_config_interim.joineui), true);

		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_appkey(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_appkey(shell);
		return 0;
	}

	if (argc == 2) {
		int ret = ctr_hex2buf(argv[1], m_config_interim.appkey,
				      sizeof(m_config_interim.appkey), true);

		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_nwkskey(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_nwkskey(shell);
		return 0;
	}

	if (argc == 2) {
		int ret = ctr_hex2buf(argv[1], m_config_interim.nwkskey,
				      sizeof(m_config_interim.nwkskey), true);

		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_appskey(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_appskey(shell);
		return 0;
	}

	if (argc == 2) {
		int ret = ctr_hex2buf(argv[1], m_config_interim.appskey,
				      sizeof(m_config_interim.appskey), true);

		if (ret >= 0) {
			return 0;
		}
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_start(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;

	ret = ctr_lrw_start(&corr_id);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_join(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;

	ret = ctr_lrw_join(&corr_id);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lrw_join` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_test_uart(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 2 && strcmp(argv[1], "enable") == 0) {
		ret = ctr_lrw_talk_enable();
		if (ret) {
			LOG_ERR("Call `ctr_lrw_talk_enable` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "disable") == 0) {
		ret = ctr_lrw_talk_disable();
		if (ret) {
			LOG_ERR("Call `ctr_lrw_talk_disable` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_test_reset(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!m_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = ctr_lrw_if_reset(dev_lrw_if);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_if_reset` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_test_cmd(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_error(shell, "only one argument is accepted (use quotes?)");
		shell_help(shell);
		return -EINVAL;
	}

	if (!m_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = ctr_lrw_talk_(argv[1]);
	if (ret) {
		LOG_ERR("Call `lrw_talk_` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lrw_config,
	SHELL_CMD_ARG(show, NULL, "List current configuration.", cmd_config_show, 1, 0),
	SHELL_CMD_ARG(test, NULL, "Get/Set LoRaWAN test mode.", cmd_config_test, 1, 1),
	SHELL_CMD_ARG(antenna, NULL, "Get/Set LoRaWAN antenna (format: <int|ext>).",
		      cmd_config_antenna, 1, 1),
	SHELL_CMD_ARG(band, NULL,
		      "Get/Set radio band"
		      " (format: <as923|au915|eu868|kr920|in865|us915>).",
		      cmd_config_band, 1, 1),
	SHELL_CMD_ARG(chmask, NULL, "Get/Set channel mask (format: <N hexadecimal digits>). ",
		      cmd_config_chmask, 1, 1),
	SHELL_CMD_ARG(class, NULL, "Get/Set device class (format: <a|c>).", cmd_config_class, 1, 1),
	SHELL_CMD_ARG(mode, NULL, "Get/Set operation mode (format: <abp|otaa>).", cmd_config_mode,
		      1, 1),
	SHELL_CMD_ARG(nwk, NULL, "Get/Set network type (format: <private|public>).", cmd_config_nwk,
		      1, 1),
	SHELL_CMD_ARG(adr, NULL, "Get/Set adaptive data rate (format: <true|false>).",
		      cmd_config_adr, 1, 1),
	SHELL_CMD_ARG(datarate, NULL, "Get/Set data rate (format: <number 1-15>).",
		      cmd_config_datarate, 1, 1),
	SHELL_CMD_ARG(dutycycle, NULL, "Get/Set duty cycle (format: <true|false>).",
		      cmd_config_dutycycle, 1, 1),
	SHELL_CMD_ARG(devaddr, NULL, "Get/Set DevAddr (format: <8 hexadecimal digits>).",
		      cmd_config_devaddr, 1, 1),
	SHELL_CMD_ARG(deveui, NULL, "Get/Set DevEUI (format: <16 hexadecimal digits>).",
		      cmd_config_deveui, 1, 1),
	SHELL_CMD_ARG(joineui, NULL, "Get/Set JoinEUI (format: <16 hexadecimal digits>).",
		      cmd_config_joineui, 1, 1),
	SHELL_CMD_ARG(appkey, NULL, "Get/Set AppKey (format: <32 hexadecimal digits>).",
		      cmd_config_appkey, 1, 1),
	SHELL_CMD_ARG(nwkskey, NULL, "Get/Set NwkSKey (format: <32 hexadecimal digits>).",
		      cmd_config_nwkskey, 1, 1),
	SHELL_CMD_ARG(appskey, NULL, "Get/Set AppSKey (format: <32 hexadecimal digits>).",
		      cmd_config_appskey, 1, 1),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lrw_test,
	SHELL_CMD_ARG(uart, NULL, "Enable/Disable UART interface (format: <enable|disable>).",
		      cmd_test_uart, 2, 0),
	SHELL_CMD_ARG(reset, NULL, "Reset modem.", cmd_test_reset, 1, 0),
	SHELL_CMD_ARG(cmd, NULL, "Send command to modem. (format: <command>)", cmd_test_cmd, 2, 0),
	SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lrw,
			       SHELL_CMD_ARG(config, &sub_lrw_config, "Configuration commands.",
					     print_help, 1, 0),
			       SHELL_CMD_ARG(start, NULL, "Start interface.", cmd_start, 1, 0),
			       SHELL_CMD_ARG(join, NULL, "Join network.", cmd_join, 1, 0),
			       SHELL_CMD_ARG(test, &sub_lrw_test, "Test commands.", print_help, 1,
					     0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(lrw, &sub_lrw, "LoRaWAN commands.", print_help);

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	k_poll_signal_init(&m_boot_sig);
	k_poll_signal_init(&m_join_sig);
	k_poll_signal_init(&m_send_sig);

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);
	if (ret) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);
	if (ret) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, cmd_config_show);

	ret = ctr_lrw_talk_init(talk_handler);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_talk_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_lrw_if_reset(dev_lrw_if);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_if_reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
