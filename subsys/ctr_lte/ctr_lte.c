#include <ctr_lte.h>
#include <ctr_lte_parse.h>
#include <ctr_lte_talk.h>
#include <ctr_config.h>
#include <ctr_rtc.h>
#include <drivers/ctr_lte_if.h>
#include <drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <init.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte, CONFIG_CTR_LTE_LOG_LEVEL);

#define SETTINGS_PFX "lte"

#define BOOT_RETRY_COUNT 3
#define BOOT_RETRY_DELAY K_SECONDS(10)
#define SETUP_RETRY_COUNT 1
#define SETUP_RETRY_DELAY K_SECONDS(10)
#define ATTACH_RETRY_COUNT 3
#define ATTACH_RETRY_DELAY K_SECONDS(60)
#define SEND_RETRY_COUNT 3
#define SEND_RETRY_DELAY K_SECONDS(10)

#define CMD_MSGQ_MAX_ITEMS 16
#define SEND_MSGQ_MAX_ITEMS 16

enum cmd_msgq_req {
	CMD_MSGQ_REQ_START = 0,
	CMD_MSGQ_REQ_ATTACH = 1,
	CMD_MSGQ_REQ_DETACH = 2,
};

struct cmd_msgq_item {
	int corr_id;
	enum cmd_msgq_req req;
};

struct send_msgq_data {
	int64_t ttl;
	uint8_t addr[4];
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

struct config {
	bool test;
	enum antenna antenna;
	bool autoconn;
	char plmnid[6 + 1];
	bool clksync;
};

static const struct device *dev_lte_if = DEVICE_DT_GET(DT_CHOSEN(ctr_lte_if));
static const struct device *dev_rfmux = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

static enum state m_state = STATE_INIT;

static struct config m_config_interim = {
	.antenna = ANTENNA_INT,
#if IS_ENABLED(CONFIG_CTR_LTE_CLKSYNC)
	.clksync = true,
#endif
	.plmnid = "23003",
};

static struct config m_config;

static struct k_poll_signal m_boot_sig;
static struct k_poll_signal m_sim_card_sig;
static struct k_poll_signal m_time_sig;
static struct k_poll_signal m_attach_sig;

K_MSGQ_DEFINE(m_cmd_msgq, sizeof(struct cmd_msgq_item), CMD_MSGQ_MAX_ITEMS, 4);
K_MSGQ_DEFINE(m_send_msgq, sizeof(struct send_msgq_item), SEND_MSGQ_MAX_ITEMS, 4);

static ctr_lte_event_cb m_event_cb;
static void *m_event_user_data;
static atomic_t m_corr_id;

static K_MUTEX_DEFINE(m_mut);
static uint64_t m_imsi;

static bool m_setup = true;

static void talk_handler(enum ctr_lte_talk_event event)
{
	switch (event) {
	case CTR_LTE_TALK_EVENT_BOOT:
		LOG_DBG("Event `CTR_LTE_TALK_EVENT_BOOT`");
		k_poll_signal_raise(&m_boot_sig, 0);
		break;

	case CTR_LTE_TALK_EVENT_SIM_CARD:
		LOG_DBG("Event `CTR_LTE_TALK_EVENT_SIM_CARD`");
		k_poll_signal_raise(&m_sim_card_sig, 0);
		break;

	case CTR_LTE_TALK_EVENT_TIME:
		LOG_DBG("Event `CTR_LTE_TALK_EVENT_TIME`");
		k_poll_signal_raise(&m_time_sig, 0);
		break;

	case CTR_LTE_TALK_EVENT_ATTACH:
		LOG_DBG("Event `CTR_LTE_TALK_EVENT_ATTACH`");
		k_poll_signal_raise(&m_attach_sig, 0);
		break;

	case CTR_LTE_TALK_EVENT_DETACH:
		LOG_DBG("Event `CTR_LTE_TALK_EVENT_DETACH`");
		/* TODO Handle event */
		break;

	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

static int wake_up(void)
{
	int ret;

	k_poll_signal_reset(&m_boot_sig);

	ctr_lte_if_wakeup(dev_lte_if);

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_boot_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(1000));

	if (ret == -EAGAIN) {
		LOG_WRN("Boot message timed out");

		/* Return positive error code intentionally */
		return ETIMEDOUT;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int boot_once(void)
{
	int ret;

	ctr_lte_if_reset(dev_lte_if);

	k_sleep(K_MSEC(1000));

	ret = wake_up();

	if (ret < 0) {
		LOG_ERR("Call `wake_up` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int boot(int retries, k_timeout_t delay)
{
	int ret;

	LOG_INF("Operation BOOT started");

	while (retries-- > 0) {
		ret = ctr_lte_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = boot_once();

		if (ret == 0) {
			ret = ctr_lte_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
				return ret;
			}

			LOG_INF("Operation BOOT succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `boot_once` failed: %d", ret);
		}

		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
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

	ret = ctr_lte_talk_at();

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at` failed: %d", ret);
		return ret;
	}

	char buf[64];

	ret = ctr_lte_talk_at_hwversion(buf, sizeof(buf));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_hwversion` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW version: %s", buf);

	ret = ctr_lte_talk_at_shortswver(buf, sizeof(buf));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_shortswver` failed: %d", ret);
		return ret;
	}

	LOG_INF("SW version: %s", buf);

	ret = ctr_lte_talk_at_xpofwarn(1, 30);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xpofwarn` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsystemmode(0, 1, 0, 0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsystemmode` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_rel14feat(1, 1, 1, 1, 0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_rel14feat` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_rai(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_rai` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xdataprfl(0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xdataprfl` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsim(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsim` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xnettime(1, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xnettime` failed: %d", ret);
		return ret;
	}

	if (m_config.clksync) {
		ret = ctr_lte_talk_at_xtime(1);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_at_xtime` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_lte_talk_at_cereg(5);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cereg` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_cgerep(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cgerep` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_cscon(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cscon` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_cpsms((int[]){ 1 }, "00111000", "00000000");

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cpsms` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_cnec(24);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cnec` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_cmee(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cmee` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_ceppi(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_ceppi` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsleep(2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsleep` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int setup(int retries, k_timeout_t delay)
{
	int ret;

	ret = boot(BOOT_RETRY_COUNT, BOOT_RETRY_DELAY);

	if (ret < 0) {
		LOG_ERR("Call `boot` failed: %d", ret);
		return ret;
	}

	LOG_INF("Operation SETUP started");

	while (retries-- > 0) {
		ret = ctr_lte_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = setup_once();

		if (ret == 0) {
			ret = ctr_lte_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
				return ret;
			}

			m_setup = false;

			LOG_INF("Operation SETUP succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `setup_once` failed: %d", ret);
		}

		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
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

static int attach_once(void)
{
	int ret;

	ret = wake_up();

	if (ret < 0) {
		LOG_ERR("Call `wake_up` failed: %d", ret);
		return ret;
	}

	k_poll_signal_reset(&m_sim_card_sig);
	k_poll_signal_reset(&m_time_sig);
	k_poll_signal_reset(&m_attach_sig);

	/* Enabled bands: B08, B20, B28 */
	const char *bands =
	        /* B88 - B81 */
	        "00000000"
	        /* B80 - B71 */
	        "0000000000"
	        /* B70 - B61 */
	        "0000000000"
	        /* B60 - B51 */
	        "0000000000"
	        /* B50 - B41 */
	        "0000000000"
	        /* B40 - B31 */
	        "0000000000"
	        /* B30 - B21 */
	        "0010000000"
	        /* B20 - B11 */
	        "1000000000"
	        /* B10 - B01 */
	        "0010000000";

	ret = ctr_lte_talk_at_xbandlock(1, bands);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xbandlock` failed: %d", ret);
		return ret;
	}

	if (!m_config.autoconn) {
		ret = ctr_lte_talk_at_cops(1, (int[]){ 2 }, m_config.plmnid);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_at_cops` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_lte_talk_at_cfun(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events_1[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
		                         &m_sim_card_sig),
	};

	ret = k_poll(events_1, ARRAY_SIZE(events_1), K_SECONDS(10));

	if (ret == -EAGAIN) {
		LOG_WRN("SIM card detection timed out");
		return -ETIMEDOUT;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	char imsi[32];

	ret = ctr_lte_talk_at_cimi(imsi, sizeof(imsi));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cimi` failed: %d", ret);
		return ret;
	}

	LOG_INF("IMSI: %s", imsi);

	k_mutex_lock(&m_mut, K_FOREVER);
	m_imsi = strtoull(imsi, NULL, 10);
	k_mutex_unlock(&m_mut);

	struct k_poll_event events_2[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
		                         &m_attach_sig),
	};

	ret = k_poll(events_2, ARRAY_SIZE(events_2), K_MINUTES(5));

	if (ret == -EAGAIN) {
		LOG_WRN("Network registration timed out");
		return -ETIMEDOUT;
	}

	if (m_config.clksync) {
		struct k_poll_event events_3[] = {
			K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
			                         &m_time_sig),
		};

		ret = k_poll(events_3, ARRAY_SIZE(events_3), K_MINUTES(10));

		if (ret == -EAGAIN) {
			LOG_WRN("Network time synchronization timed out");
			return -ETIMEDOUT;
		}

		if (ret < 0) {
			LOG_ERR("Call `k_poll` failed: %d", ret);
			return ret;
		}

		char cclk[64];

		ret = ctr_lte_talk_at_cclk_q(cclk, sizeof(cclk));

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_at_cclk_q` failed: %d", ret);
			return ret;
		}

#if defined(CONFIG_CTR_RTC)
		struct ctr_rtc_tm tm;

		ret = ctr_lte_parse_cclk(cclk, &tm.year, &tm.month, &tm.day, &tm.hours, &tm.minutes,
		                         &tm.seconds);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_parse_cclk` failed: %d", ret);
			return ret;
		}

		tm.year += 2000;

		ret = ctr_rtc_set(&tm);

		if (ret < 0) {
			LOG_ERR("Call `ctr_rtc_set` failed: %d", ret);
			return ret;
		}
#else
		ret = ctr_lte_parse_cclk(cclk, NULL, NULL, NULL, NULL, NULL, NULL);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_parse_cclk` failed: %d", ret);
			return ret;
		}
#endif
	}

	char xsocket[32];

	ret = ctr_lte_talk_at_xsocket(1, (int[]){ 2 }, (int[]){ 0 }, xsocket, sizeof(xsocket));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsocket` failed: %d", ret);
		return ret;
	}

	if (strcmp(xsocket, "#XSOCKET: 1,2,17") != 0) {
		LOG_ERR("Unexpected socket response");
		return -ENOTCONN;
	}

	ret = ctr_lte_talk_at_xsleep(2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsleep` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int attach(int retries, k_timeout_t delay)
{
	int ret;

	LOG_INF("Operation ATTACH started");

	while (retries-- > 0) {
		if (m_setup) {
			ret = setup(SETUP_RETRY_COUNT, SETUP_RETRY_DELAY);

			if (ret < 0) {
				LOG_ERR("Call `setup` failed: %d", ret);
				return ret;
			}
		}

		ret = ctr_lte_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			m_setup = true;
			return ret;
		}

		ret = attach_once();

		if (ret == 0) {
			ret = ctr_lte_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
				m_setup = true;
				return ret;
			}

			LOG_INF("Operation ATTACH succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `attach_once` failed: %d", ret);
			m_setup = true;
		}

		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
			m_setup = true;
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating ATTACH operation (retries left: %d)", retries);
		}
	}

	m_setup = true;

	LOG_WRN("Operation ATTACH failed");
	return -ENOTCONN;
}

static int send_once(const struct send_msgq_data *data)
{
	int ret;

	ret = wake_up();

	if (ret < 0) {
		LOG_ERR("Call `wake_up` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsocketopt(1, 50, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsocketopt` failed: %d", ret);
		return ret;
	}

	char addr[15 + 1];

	snprintf(addr, sizeof(addr), "%d.%d.%d.%d", data->addr[0], data->addr[1], data->addr[2],
	         data->addr[3]);

	ret = ctr_lte_talk_at_xsendto(addr, data->port, data->buf, data->len);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsendto` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsleep(2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsleep` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int send(int retries, k_timeout_t delay, const struct send_msgq_data *data)
{
	int ret;
	int err = 0;

	LOG_INF("Operation SEND started");

	while (retries-- > 0) {
		if (m_setup) {
			ret = setup(SETUP_RETRY_COUNT, SETUP_RETRY_DELAY);

			if (ret < 0) {
				LOG_ERR("Call `setup` failed: %d", ret);
				return ret;
			}
		}

		ret = ctr_lte_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			m_setup = true;
			return ret;
		}

		ret = send_once(data);
		err = ret;

		if (ret == 0) {
			ret = ctr_lte_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
				m_setup = true;
				return ret;
			}

			LOG_INF("Operation SEND succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `send_once` failed: %d", ret);
		}

		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
			m_setup = true;
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating SEND operation (retries left: %d)", retries);
		}
	}

	m_setup = true;

	LOG_WRN("Operation SEND failed");
	return err;
}

static int start(void)
{
	int ret;

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

	union ctr_lte_event_data data = { 0 };

	ret = start();

	if (ret < 0) {
		LOG_ERR("Call `start` failed: %d", ret);

		data.start_err.corr_id = item->corr_id;

		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_EVENT_START_ERR, &data, m_event_user_data);
		}

		return ret;
	}

	m_state = STATE_READY;

	data.start_ok.corr_id = item->corr_id;

	if (m_event_cb != NULL) {
		m_event_cb(CTR_LTE_EVENT_START_OK, &data, m_event_user_data);
	}

	return 0;
}

static int process_req_attach(const struct cmd_msgq_item *item)
{
	int ret;

	union ctr_lte_event_data data = { 0 };

	ret = attach(ATTACH_RETRY_COUNT, ATTACH_RETRY_DELAY);

	if (ret < 0) {
		LOG_ERR("Call `attach` failed: %d", ret);

		data.attach_err.corr_id = item->corr_id;

		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_EVENT_ATTACH_ERR, &data, m_event_user_data);
		}

		return ret;
	}

	data.attach_ok.corr_id = item->corr_id;

	if (m_event_cb != NULL) {
		m_event_cb(CTR_LTE_EVENT_ATTACH_OK, &data, m_event_user_data);
	}

	return 0;
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

				if (m_event_cb != NULL) {
					m_event_cb(CTR_LTE_EVENT_FAILURE, NULL, m_event_user_data);
				}
			}
		}

	} else if (item.req == CMD_MSGQ_REQ_ATTACH) {
		LOG_INF("Dequeued ATTACH command (correlation id: %d)", item.corr_id);

		if (m_state != STATE_READY) {
			LOG_WRN("Not ready for JOIN command - ignoring");

		} else {
			ret = process_req_attach(&item);

			m_state = ret < 0 ? STATE_ERROR : STATE_READY;

			if (m_state == STATE_ERROR) {
				LOG_ERR("Error - flushing all control commands from queue");

				k_msgq_purge(&m_cmd_msgq);

				if (m_event_cb != NULL) {
					m_event_cb(CTR_LTE_EVENT_FAILURE, NULL, m_event_user_data);
				}
			}
		}

	} else {
		LOG_ERR("Unknown message: %d", (int)item.req);
	}
}

static int process_req_send(const struct send_msgq_item *item)
{
	int ret;

	union ctr_lte_event_data data = { 0 };

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

		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_EVENT_SEND_ERR, &data, m_event_user_data);
		}

		return ret;
	}

	data.send_ok.corr_id = item->corr_id;

	if (m_event_cb != NULL) {
		m_event_cb(CTR_LTE_EVENT_SEND_OK, &data, m_event_user_data);
	}

	return 0;
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

	LOG_INF("Dequeued SEND command (correlation id: %d)", item.corr_id);

	ret = process_req_send(&item);

	m_state = ret < 0 ? STATE_ERROR : STATE_READY;

	if (m_state == STATE_ERROR) {
		LOG_ERR("Error - flushing all control commands from queue");

		k_msgq_purge(&m_cmd_msgq);

		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_EVENT_FAILURE, NULL, m_event_user_data);
		}
	}
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

K_THREAD_DEFINE(ctr_lte, CONFIG_CTR_LTE_THREAD_STACK_SIZE, dispatcher_thread, NULL, NULL, NULL,
                CONFIG_CTR_LTE_THREAD_PRIORITY, 0, 0);

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
	SETTINGS_SET("autoconn", &m_config_interim.autoconn, sizeof(m_config_interim.autoconn));
	SETTINGS_SET("plmnid", m_config_interim.plmnid, sizeof(m_config_interim.plmnid));
	SETTINGS_SET("clksync", &m_config_interim.clksync, sizeof(m_config_interim.clksync));

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
	EXPORT_FUNC("autoconn", &m_config_interim.autoconn, sizeof(m_config_interim.autoconn));
	EXPORT_FUNC("plmnid", m_config_interim.plmnid, sizeof(m_config_interim.plmnid));
	EXPORT_FUNC("clksync", &m_config_interim.clksync, sizeof(m_config_interim.clksync));

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

static void print_autoconn(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config autoconn %s",
	            m_config_interim.autoconn ? "true" : "false");
}

static void print_plmnid(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config plmnid %s", m_config_interim.plmnid);
}

static void print_clksync(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config clksync %s",
	            m_config_interim.clksync ? "true" : "false");
}

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_test(shell);
	print_antenna(shell);
	print_autoconn(shell);
	print_plmnid(shell);
	print_clksync(shell);

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

static int cmd_config_autoconn(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_autoconn(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.autoconn = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.autoconn = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_plmnid(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_plmnid(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len != 5 && len != 6) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		memcpy(m_config_interim.plmnid, argv[1], len);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_clksync(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_clksync(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.clksync = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.clksync = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_get_imsi(uint64_t *imsi)
{
	k_mutex_lock(&m_mut, K_FOREVER);

	if (m_imsi == 0) {
		k_mutex_unlock(&m_mut);
		return -ENODATA;
	}

	*imsi = m_imsi;

	k_mutex_unlock(&m_mut);

	return 0;
}

int ctr_lte_set_event_cb(ctr_lte_event_cb cb, void *user_data)
{
	m_event_cb = cb;
	m_event_user_data = user_data;

	return 0;
}

int ctr_lte_start(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_ERR("Test mode activated - ignoring");

		/* Return positive error code intentionally */
		return ENOEXEC;
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

int ctr_lte_attach(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_ERR("Test mode activated - ignoring");

		/* Return positive error code intentionally */
		return ENOEXEC;
	}

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_ATTACH,
	};

	if (corr_id != NULL) {
		*corr_id = item.corr_id;
	}

	LOG_INF("Enqueing ATTACH command (correlation id: %d)", item.corr_id);

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_detach(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_ERR("Test mode activated - ignoring");

		/* Return positive error code intentionally */
		return ENOEXEC;
	}

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_DETACH,
	};

	if (corr_id != NULL) {
		*corr_id = item.corr_id;
	}

	LOG_INF("Enqueing DETACH command (correlation id: %d)", item.corr_id);

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);

	if (ret < 0) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_lte_send(const struct ctr_lte_send_opts *opts, const void *buf, size_t len, int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_ERR("Test mode activated - ignoring");

		/* Return positive error code intentionally */
		return ENOEXEC;
	}

	void *p = k_malloc(len);

	if (p == NULL) {
		LOG_ERR("Call `k_malloc` failed");
		return -ENOMEM;
	}

	memcpy(p, buf, len);

	struct send_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.data = { .ttl = opts->ttl,
		          .port = opts->port,
		          .buf = p,
		          .len = len, },
	};

	memcpy(item.data.addr, opts->addr, sizeof(item.data.addr));

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

static int cmd_start(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;

	ret = ctr_lte_start(&corr_id);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_attach(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;

	ret = ctr_lte_attach(&corr_id);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
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
		ret = ctr_lte_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "disable") == 0) {
		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
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
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	if (!m_config.test) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ctr_lte_if_reset(dev_lte_if);

	k_sleep(K_MSEC(1000));

	return 0;
}

static int cmd_test_wakeup(const struct shell *shell, size_t argc, char **argv)
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

	k_poll_signal_reset(&m_boot_sig);

	ctr_lte_if_wakeup(dev_lte_if);

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_boot_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(1000));

	if (ret == -EAGAIN) {
		LOG_WRN("Boot message timed out");
		shell_warn(shell, "boot message timed out");

		/* Return positive error code intentionally */
		return ETIMEDOUT;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
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

	ret = ctr_lte_talk_(argv[1]);

	if (ret < 0) {
		LOG_ERR("Call `lte_talk_` failed: %d", ret);
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
        sub_lte_config,
        SHELL_CMD_ARG(show, NULL, "List current configuration.", cmd_config_show, 1, 0),
        SHELL_CMD_ARG(test, NULL, "Get/Set LTE test mode.", cmd_config_test, 1, 1),
        SHELL_CMD_ARG(antenna, NULL, "Get/Set LTE antenna (format: <int|ext>).", cmd_config_antenna,
                      1, 1),
        SHELL_CMD_ARG(autoconn, NULL, "Get/Set auto-connect feature (format: <true|false>).",
                      cmd_config_autoconn, 1, 1),
        SHELL_CMD_ARG(plmnid, NULL, "Get/Set network PLMN ID (format: 5-6 digits).",
                      cmd_config_plmnid, 1, 1),
        SHELL_CMD_ARG(clksync, NULL, "Get/Set clock synchronization (format: <true|false>).",
                      cmd_config_clksync, 1, 1),
        SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_lte_test,
        SHELL_CMD_ARG(uart, NULL, "Enable/Disable UART interface (format: <enable|disable>).",
                      cmd_test_uart, 2, 0),
        SHELL_CMD_ARG(reset, NULL, "Reset modem.", cmd_test_reset, 1, 0),
        SHELL_CMD_ARG(wakeup, NULL, "Wake up modem.", cmd_test_wakeup, 1, 0),
        SHELL_CMD_ARG(cmd, NULL, "Send command to modem. (format: <command>)", cmd_test_cmd, 2, 0),
        SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lte,
                               SHELL_CMD_ARG(config, &sub_lte_config, "Configuration commands.",
                                             print_help, 1, 0),
                               SHELL_CMD_ARG(start, NULL, "Start interface.", cmd_start, 1, 0),
                               SHELL_CMD_ARG(attach, NULL, "Attach to network.", cmd_attach, 1, 0),
                               SHELL_CMD_ARG(test, &sub_lte_test, "Test commands.", print_help, 1,
                                             0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(lte, &sub_lte, "LTE commands.", print_help);

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	k_poll_signal_init(&m_boot_sig);
	k_poll_signal_init(&m_sim_card_sig);
	k_poll_signal_init(&m_time_sig);
	k_poll_signal_init(&m_attach_sig);

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);

	if (ret < 0) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);

	if (ret < 0) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, cmd_config_show);

	ret = ctr_rfmux_set_antenna(dev_rfmux, m_config.antenna == ANTENNA_EXT ?
                                                       CTR_RFMUX_ANTENNA_EXT :
                                                       CTR_RFMUX_ANTENNA_INT);

	if (ret < 0) {
		LOG_ERR("Call `ctr_rfmux_set_antenna` failed: %d", ret);
		return ret;
	}

	ret = ctr_rfmux_set_interface(dev_rfmux, CTR_RFMUX_INTERFACE_LTE);

	if (ret < 0) {
		LOG_ERR("Call `ctr_rfmux_set_interface` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_init(talk_handler);
	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
