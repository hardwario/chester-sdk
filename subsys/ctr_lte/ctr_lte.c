/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_parse.h"
#include "ctr_lte_talk.h"

#include <chester/ctr_config.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_rtc.h>
#include <chester/drivers/ctr_lte_if.h>
#include <chester/drivers/ctr_rfmux.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define K_MSGQ_DEFINE_STATIC(q_name, q_msg_size, q_max_msgs, q_align)                              \
	static char __noinit __aligned(q_align) _k_fifo_buf_##q_name[(q_max_msgs) * (q_msg_size)]; \
	static STRUCT_SECTION_ITERABLE(k_msgq, q_name) =                                           \
		Z_MSGQ_INITIALIZER(q_name, _k_fifo_buf_##q_name, q_msg_size, q_max_msgs)

LOG_MODULE_REGISTER(ctr_lte, CONFIG_CTR_LTE_LOG_LEVEL);

#define SETTINGS_PFX "lte"

#define XSLEEP_PAUSE	  K_MSEC(100)
#define BOOT_TIMEOUT	  K_SECONDS(5)
#define BOOT_RETRY_COUNT  3
#define BOOT_RETRY_DELAY  K_SECONDS(10)
#define SETUP_RETRY_COUNT 1
#define SETUP_RETRY_DELAY K_SECONDS(10)
#define EVAL_RETRY_COUNT  3
#define EVAL_RETRY_DELAY  K_SECONDS(3)
#define SEND_RETRY_COUNT  3
#define SEND_RETRY_DELAY  K_SECONDS(10)

/*
 * Table of the attach retry delays
 *
 * Column 1: Timeout between previous and current attempt
 * Column 2: Attach attempt duration
 */

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

#define CMD_MSGQ_MAX_ITEMS  16
#define SEND_MSGQ_MAX_ITEMS 16

enum cmd_msgq_req {
	CMD_MSGQ_REQ_START = 0,
	CMD_MSGQ_REQ_ATTACH = 1,
	CMD_MSGQ_REQ_DETACH = 2,
	CMD_MSGQ_REQ_EVAL = 3,
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

enum auth {
	AUTH_NONE = 0,
	AUTH_PAP = 1,
	AUTH_CHAP = 2,
};

struct config {
	bool test;
	enum antenna antenna;
	bool nb_iot_mode;
	bool lte_m_mode;
	bool autoconn;
	char plmnid[6 + 1];
	bool clksync;
	char apn[63 + 1];
	enum auth auth;
	char username[32 + 1];
	char password[32 + 1];
	uint8_t addr[4];
	int port;
};

static const struct device *dev_lte_if = DEVICE_DT_GET(DT_CHOSEN(ctr_lte_if));
static const struct device *dev_rfmux = DEVICE_DT_GET(DT_NODELABEL(ctr_rfmux));

static enum state m_state = STATE_INIT;

static struct config m_config_interim = {
	.antenna = ANTENNA_INT,
	.nb_iot_mode = true,
#if defined(CONFIG_CTR_LTE_CLKSYNC)
	.clksync = true,
#endif
	.plmnid = "23003",
	.apn = "hardwario.com",
	.auth = AUTH_NONE,
	.addr = {192, 168, 168, 1},
	.port = 10000,
};

static struct config m_config;

static struct k_poll_signal m_boot_sig;
static struct k_poll_signal m_sim_card_sig;
static struct k_poll_signal m_time_sig;
static struct k_poll_signal m_attach_sig;

K_MSGQ_DEFINE_STATIC(m_cmd_msgq, sizeof(struct cmd_msgq_item), CMD_MSGQ_MAX_ITEMS, 4);
K_MSGQ_DEFINE_STATIC(m_send_msgq, sizeof(struct send_msgq_item), SEND_MSGQ_MAX_ITEMS, 4);

static ctr_lte_event_cb m_event_cb;
static void *m_event_user_data;
static atomic_t m_corr_id;

static K_MUTEX_DEFINE(m_mut);
static atomic_t m_attached;
static uint64_t m_imei;
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

	ret = ctr_lte_if_wakeup(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_wakeup` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_boot_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), BOOT_TIMEOUT);
	if (ret == -EAGAIN) {
		LOG_WRN("Boot message timed out");

		/* Return positive error code intentionally */
		return ETIMEDOUT;
	}

	if (ret) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int boot_once(void)
{
	int ret;

	ret = ctr_lte_if_reset(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_reset` failed: %d", ret);
		return ret;
	}

	ret = wake_up();
	if (ret) {
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

	ret = ctr_lte_talk_at_cfun(0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cfun` failed: %d", ret);
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

	ret = ctr_lte_talk_at_xsystemmode(m_config.lte_m_mode ? 1 : 0, m_config.nb_iot_mode ? 1 : 0,
					  0, 0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsystemmode` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_rel14feat(1, 1, 1, 1, 0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_rel14feat` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_mdmev(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_mdmev` failed: %d", ret);
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

	ret = ctr_lte_talk_at_cpsms((int[]){1}, "00111000", "00000000");

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

	k_sleep(XSLEEP_PAUSE);

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

static int attach_once(k_timeout_t timeout)
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

	/* Enabled bands: B2, B4, B5, B8, B12, B20, B28 */
#if 1
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
		"1000000010"
		/* B10 -  B1 */
		"0010011010";
#else
	const char *bands =
		/* B88 - B81 */
		"111111"
		/* B80 - B71 */
		"1111111111"
		/* B70 - B61 */
		"1111111111"
		/* B60 - B51 */
		"1111111111"
		/* B50 - B41 */
		"1111111111"
		/* B40 - B31 */
		"1111111111"
		/* B30 - B21 */
		"1111111111"
		/* B20 - B11 */
		"1111111111"
		/* B10 - B01 */
		"1111111111";
#endif

	ret = ctr_lte_talk_at_xbandlock(1, bands);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xbandlock` failed: %d", ret);
		return ret;
	}

	if (!m_config.autoconn) {
		ret = ctr_lte_talk_at_cops(1, (int[]){2}, m_config.plmnid);
		if (ret) {
			LOG_ERR("Call `ctr_lte_talk_at_cops` failed: %d", ret);
			return ret;
		}
	} else {
		ret = ctr_lte_talk_at_cops(0, NULL, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lte_talk_at_cops` failed: %d", ret);
			return ret;
		}
	}

	if (strlen(m_config.apn)) {
		ret = ctr_lte_talk_at_cgdcont(0, "IP", m_config.apn);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_at_cgdcont` failed: %d", ret);
			return ret;
		}
	}

	if (m_config.auth == AUTH_PAP || m_config.auth == AUTH_CHAP) {
		int protocol = m_config.auth == AUTH_PAP ? 1 : 2;
		ret = ctr_lte_talk_at_cgauth(0, &protocol, m_config.username, m_config.password);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_at_cgauth` failed: %d", ret);
			return ret;
		}
	} else {
		ret = ctr_lte_talk_at_cgauth(0, (int[]){0}, NULL, NULL);

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_at_cgauth` failed: %d", ret);
			return ret;
		}
	}

cfun_1:
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

	char crsm_144[32];
	ret = ctr_lte_talk_at_crsm_176(crsm_144, sizeof(crsm_144));
	if (ret) {
		LOG_ERR("Call `ctr_lte_talk_at_crsm_176` failed: %d", ret);
		return ret;
	}

	if (strcmp(crsm_144, "FFFFFFFFFFFFFFFFFFFFFFFF")) {
		LOG_WRN("Found forbidden network(s) - erasing");

		ret = ctr_lte_talk_at_crsm_214();
		if (ret) {
			LOG_ERR("Call `ctr_lte_talk_at_crsm_214` failed: %d", ret);
			return ret;
		}

		ret = ctr_lte_talk_at_cfun(4);
		if (ret) {
			LOG_ERR("Call `ctr_lte_talk_at_cfun` failed: %d", ret);
			return ret;
		}

		goto cfun_1;
	}

	char imei[32];

	ret = ctr_lte_talk_at_cgsn(imei, sizeof(imei));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cgsn` failed: %d", ret);
		return ret;
	}

	LOG_INF("IMEI: %s", imei);

	k_mutex_lock(&m_mut, K_FOREVER);
	m_imei = strtoull(imei, NULL, 10);
	k_mutex_unlock(&m_mut);

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

	ret = k_poll(events_2, ARRAY_SIZE(events_2), timeout);

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

		ret = ctr_rtc_set_tm(&tm);

		if (ret < 0) {
			LOG_ERR("Call `ctr_rtc_set_tm` failed: %d", ret);
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

	ret = ctr_lte_talk_at_xsocket(1, (int[]){2}, (int[]){0}, xsocket, sizeof(xsocket));

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsocket` failed: %d", ret);
		return ret;
	}

	/* TODO We accept both answers as correct since there was a change in the SLM application */
	if (strcmp(xsocket, "#XSOCKET: 1,2,17") != 0 && strcmp(xsocket, "#XSOCKET: 0,2,17") != 0) {
		LOG_ERR("Unexpected socket response");
		return -ENOTCONN;
	}

	ret = ctr_lte_talk_at_xsleep(2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsleep` failed: %d", ret);
		return ret;
	}

	k_sleep(XSLEEP_PAUSE);

	return 0;
}

static int attach(int retries, const k_timeout_t *delays)
{
	int ret;

	LOG_INF("Operation ATTACH started");

	atomic_set(&m_attached, false);

#define CURRENT_DELAY  0
#define ATTACH_TIMEOUT 1
#define NEXT_DELAY     2
#define TABLE_STEP     2

	for (; retries-- > 0; delays += TABLE_STEP) {
		k_sleep(delays[CURRENT_DELAY]);

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

		ret = attach_once(delays[ATTACH_TIMEOUT]);

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
			LOG_WRN("Repeating ATTACH operation in %lld minutes (retries left: %d)",
				k_ticks_to_ms_ceil64(delays[NEXT_DELAY].ticks) / 1000 / 60,
				retries);
		}
	}

	m_setup = true;

#undef CURRENT_DELAY
#undef NEXT_DELAY
#undef ATTACH_TIMEOUT
#undef TABLE_STEP

	LOG_WRN("Operation ATTACH failed");
	return -ENOTCONN;
}

static int eval_once(struct ctr_lte_eval *eval)
{
	int ret;

	ret = wake_up();
	if (ret < 0) {
		LOG_ERR("Call `wake_up` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_coneval(eval);
	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_coneval` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsleep(2);
	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsleep` failed: %d", ret);
		return ret;
	}

	k_sleep(XSLEEP_PAUSE);

	return 0;
}

static int eval(int retries, k_timeout_t delay, struct ctr_lte_eval *eval)
{
	int ret;
	int err = 0;

	LOG_INF("Operation EVAL started");

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

		ret = eval_once(eval);
		err = ret;

		if (ret == 0) {
			ret = ctr_lte_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
				m_setup = true;
				return ret;
			}

			LOG_INF("Operation EVAL succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `eval_once` failed: %d", ret);
		}

		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
			m_setup = true;
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating EVAL operation (retries left: %d)", retries);
		}
	}

	m_setup = true;

	LOG_WRN("Operation EVAL failed");
	return err;
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

	if (data->addr[0] == 0 && data->addr[1] == 0 && data->addr[2] == 0 && data->addr[3] == 0) {
		snprintf(addr, sizeof(addr), "%u.%u.%u.%u", m_config.addr[0], m_config.addr[1],
			 m_config.addr[2], m_config.addr[3]);
	} else {
		snprintf(addr, sizeof(addr), "%u.%u.%u.%u", data->addr[0], data->addr[1],
			 data->addr[2], data->addr[3]);
	}

	int port;

	if (data->port == -1) {
		port = m_config.port;
	} else {
		port = data->port;
	}

	ret = ctr_lte_talk_at_xsendto(addr, port, data->buf, data->len);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsendto` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_talk_at_xsleep(2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xsleep` failed: %d", ret);
		return ret;
	}

	k_sleep(XSLEEP_PAUSE);

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

		ret = ctr_rfmux_set_interface(dev_rfmux, CTR_RFMUX_INTERFACE_LTE);

		if (ret < 0) {
			LOG_ERR("Call `ctr_rfmux_set_interface` failed: %d", ret);
			return ret;
		}

		initialized = true;
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

	union ctr_lte_event_data data = {0};

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

	union ctr_lte_event_data data = {0};

	const k_timeout_t delays[] = ATTACH_RETRY_DELAYS;
	ret = attach(ARRAY_SIZE(delays) / 2, delays);

	if (ret < 0) {
		LOG_ERR("Call `attach` failed: %d", ret);

		data.attach_err.corr_id = item->corr_id;

		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_EVENT_ATTACH_ERR, &data, m_event_user_data);
		}

		return ret;
	}

	atomic_set(&m_attached, true);

	data.attach_ok.corr_id = item->corr_id;

	if (m_event_cb != NULL) {
		m_event_cb(CTR_LTE_EVENT_ATTACH_OK, &data, m_event_user_data);
	}

	return 0;
}

static int process_req_eval(const struct cmd_msgq_item *item)
{
	int ret;

	union ctr_lte_event_data data = {0};

	ret = eval(EVAL_RETRY_COUNT, EVAL_RETRY_DELAY, &data.eval_ok.eval);

	if (ret < 0) {
		LOG_ERR("Call `eval` failed: %d", ret);

		data.eval_err.corr_id = item->corr_id;

		if (m_event_cb != NULL) {
			m_event_cb(CTR_LTE_EVENT_EVAL_ERR, &data, m_event_user_data);
		}

		return ret;
	}

	data.eval_ok.corr_id = item->corr_id;

	if (m_event_cb != NULL) {
		m_event_cb(CTR_LTE_EVENT_EVAL_OK, &data, m_event_user_data);
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

				atomic_set(&m_attached, false);

				k_msgq_purge(&m_cmd_msgq);

				if (m_event_cb != NULL) {
					m_event_cb(CTR_LTE_EVENT_FAILURE, NULL, m_event_user_data);
				}
			}
		}

	} else if (item.req == CMD_MSGQ_REQ_ATTACH) {
		LOG_INF("Dequeued ATTACH command (correlation id: %d)", item.corr_id);

		if (m_state != STATE_READY) {
			LOG_WRN("Not ready for ATTACH command - ignoring");

		} else {
			ret = process_req_attach(&item);

			m_state = ret < 0 ? STATE_ERROR : STATE_READY;

			if (m_state == STATE_ERROR) {
				LOG_ERR("Error - flushing all control commands from queue");

				atomic_set(&m_attached, false);

				k_msgq_purge(&m_cmd_msgq);

				if (m_event_cb != NULL) {
					m_event_cb(CTR_LTE_EVENT_FAILURE, NULL, m_event_user_data);
				}
			}
		}

	} else if (item.req == CMD_MSGQ_REQ_EVAL) {
		LOG_INF("Dequeued EVAL command (correlation id: %d)", item.corr_id);

		if (m_state != STATE_READY) {
			LOG_WRN("Not ready for EVAL command - ignoring");

		} else {
			ret = process_req_eval(&item);

			m_state = ret < 0 ? STATE_ERROR : STATE_READY;

			if (m_state == STATE_ERROR) {
				LOG_ERR("Error - flushing all control commands from queue");

				atomic_set(&m_attached, false);

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

	union ctr_lte_event_data data = {0};

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

		atomic_set(&m_attached, false);

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
	SETTINGS_SET("nb-iot-mode", &m_config_interim.nb_iot_mode,
		     sizeof(m_config_interim.nb_iot_mode));
	SETTINGS_SET("lte-m-mode", &m_config_interim.lte_m_mode,
		     sizeof(m_config_interim.lte_m_mode));
	SETTINGS_SET("autoconn", &m_config_interim.autoconn, sizeof(m_config_interim.autoconn));
	SETTINGS_SET("plmnid", m_config_interim.plmnid, sizeof(m_config_interim.plmnid));
	SETTINGS_SET("clksync", &m_config_interim.clksync, sizeof(m_config_interim.clksync));
	SETTINGS_SET("apn", m_config_interim.apn, sizeof(m_config_interim.apn));
	SETTINGS_SET("auth", &m_config_interim.auth, sizeof(m_config_interim.auth));
	SETTINGS_SET("username", m_config_interim.username, sizeof(m_config_interim.username));
	SETTINGS_SET("password", m_config_interim.password, sizeof(m_config_interim.password));
	SETTINGS_SET("addr", m_config_interim.addr, sizeof(m_config_interim.addr));
	SETTINGS_SET("port", &m_config_interim.port, sizeof(m_config_interim.port));

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
	EXPORT_FUNC("nb-iot-mode", &m_config_interim.nb_iot_mode,
		    sizeof(m_config_interim.nb_iot_mode));
	EXPORT_FUNC("lte-m-mode", &m_config_interim.lte_m_mode,
		    sizeof(m_config_interim.lte_m_mode));
	EXPORT_FUNC("autoconn", &m_config_interim.autoconn, sizeof(m_config_interim.autoconn));
	EXPORT_FUNC("plmnid", m_config_interim.plmnid, sizeof(m_config_interim.plmnid));
	EXPORT_FUNC("clksync", &m_config_interim.clksync, sizeof(m_config_interim.clksync));
	EXPORT_FUNC("apn", m_config_interim.apn, sizeof(m_config_interim.apn));
	EXPORT_FUNC("auth", &m_config_interim.auth, sizeof(m_config_interim.auth));
	EXPORT_FUNC("username", m_config_interim.username, sizeof(m_config_interim.username));
	EXPORT_FUNC("password", m_config_interim.password, sizeof(m_config_interim.password));
	EXPORT_FUNC("addr", m_config_interim.addr, sizeof(m_config_interim.addr));
	EXPORT_FUNC("port", &m_config_interim.port, sizeof(m_config_interim.port));

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

static void print_nb_iot_mode(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config nb-iot-mode %s",
		    m_config_interim.nb_iot_mode ? "true" : "false");
}

static void print_lte_m_mode(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config lte-m-mode %s",
		    m_config_interim.lte_m_mode ? "true" : "false");
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

static void print_apn(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config apn %s", m_config_interim.apn);
}

static void print_auth(const struct shell *shell)
{
	switch (m_config_interim.auth) {
	case AUTH_NONE:
		shell_print(shell, SETTINGS_PFX " config auth none");
		break;
	case AUTH_CHAP:
		shell_print(shell, SETTINGS_PFX " config auth chap");
		break;
	case AUTH_PAP:
		shell_print(shell, SETTINGS_PFX " config auth pap");
		break;
	default:
		shell_print(shell, SETTINGS_PFX " config auth (unknown)");
	}
}

static void print_username(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config username %s", m_config_interim.username);
}

static void print_password(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config password %s", m_config_interim.password);
}

static void print_addr(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config addr %u.%u.%u.%u", m_config_interim.addr[0],
		    m_config_interim.addr[1], m_config_interim.addr[2], m_config_interim.addr[3]);
}

static void print_port(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config port %d", m_config_interim.port);
}

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_test(shell);
	print_antenna(shell);
	print_nb_iot_mode(shell);
	print_lte_m_mode(shell);
	print_autoconn(shell);
	print_plmnid(shell);
	print_clksync(shell);
	print_apn(shell);
	print_auth(shell);
	print_username(shell);
	print_password(shell);
	print_addr(shell);
	print_port(shell);

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

static int cmd_config_nb_iot_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_nb_iot_mode(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.nb_iot_mode = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.nb_iot_mode = false;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_lte_m_mode(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_lte_m_mode(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "true") == 0) {
		m_config_interim.lte_m_mode = true;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "false") == 0) {
		m_config_interim.lte_m_mode = false;
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

		strcpy(m_config_interim.plmnid, argv[1]);
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

static int cmd_config_apn(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_apn(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config_interim.apn)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			char c = argv[1][i];
			if (!isalnum((int)c) && c != '-' && c != '.') {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		strcpy(m_config_interim.apn, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_auth(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_auth(shell);
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "none") == 0) {
		m_config_interim.auth = AUTH_NONE;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "pap") == 0) {
		m_config_interim.auth = AUTH_PAP;
		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "chap") == 0) {
		m_config_interim.auth = AUTH_CHAP;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_username(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_username(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config_interim.username)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		strcpy(m_config_interim.username, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_password(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_password(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len >= sizeof(m_config_interim.password)) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		strcpy(m_config_interim.password, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_addr(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 1) {
		print_addr(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len > 15) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		unsigned int addr[4];

		ret = sscanf(argv[1], "%u.%u.%u.%u", &addr[0], &addr[1], &addr[2], &addr[3]);
		if (ret != 4) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		if ((addr[0] < 0 || addr[0] > 255) || (addr[1] < 0 || addr[1] > 255) ||
		    (addr[2] < 0 || addr[2] > 255) || (addr[3] < 0 || addr[3] > 255)) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_config_interim.addr[0] = addr[0];
		m_config_interim.addr[1] = addr[1];
		m_config_interim.addr[2] = addr[2];
		m_config_interim.addr[3] = addr[3];

		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

static int cmd_config_port(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_port(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len < 1 || len > 5) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		int port = strtol(argv[1], NULL, 10);

		if (port < 0 || port > 65535) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}

		m_config_interim.port = port;
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
}

int ctr_lte_set_event_cb(ctr_lte_event_cb cb, void *user_data)
{
	m_event_cb = cb;
	m_event_user_data = user_data;

	return 0;
}

int ctr_lte_get_imei(uint64_t *imei)
{
	k_mutex_lock(&m_mut, K_FOREVER);

	if (m_imei == 0) {
		k_mutex_unlock(&m_mut);
		return -ENODATA;
	}

	*imei = m_imei;

	k_mutex_unlock(&m_mut);

	return 0;
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

int ctr_lte_is_attached(bool *attached)
{
	*attached = atomic_get(&m_attached) ? true : false;

	return 0;
}

int ctr_lte_start(int *corr_id)
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

int ctr_lte_attach(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
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
		LOG_WRN("Test mode activated - ignoring");
		return 0;
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

int ctr_lte_eval(int *corr_id)
{
	int ret;

	if (m_config.test) {
		LOG_WRN("Test mode activated - ignoring");
		return 0;
	}

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_EVAL,
	};

	if (corr_id != NULL) {
		*corr_id = item.corr_id;
	}

	LOG_INF("Enqueing EVAL command (correlation id: %d)", item.corr_id);

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
		.data =
			{
				.ttl = opts->ttl,
				.port = opts->port,
				.buf = p,
				.len = len,
			},
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
	if (ret) {
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
	if (ret) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_imei(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	uint64_t imei;
	ret = ctr_lte_get_imei(&imei);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imei` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "imei: %llu", imei);

	return 0;
}

static int cmd_imsi(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	uint64_t imsi;
	ret = ctr_lte_get_imsi(&imsi);
	if (ret) {
		LOG_ERR("Call `ctr_lte_get_imsi` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "imsi: %llu", imsi);

	return 0;
}

static int cmd_state(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	bool attached;
	ret = ctr_lte_is_attached(&attached);
	if (ret) {
		LOG_ERR("Call `ctr_lte_is_attached` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "attached: %s", attached ? "yes" : "no");

	return 0;
}

static int cmd_test_uart(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc == 2 && strcmp(argv[1], "enable") == 0) {
		ret = ctr_lte_talk_enable();
		if (ret) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		return 0;
	}

	if (argc == 2 && strcmp(argv[1], "disable") == 0) {
		ret = ctr_lte_talk_disable();
		if (ret) {
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

	ret = ctr_lte_if_reset(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_reset` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

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

	ret = ctr_lte_if_wakeup(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_wakeup` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_boot_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), BOOT_TIMEOUT);
	if (ret == -EAGAIN) {
		LOG_WRN("Boot message timed out");
		shell_warn(shell, "boot message timed out");
		return 0;
	}

	if (ret) {
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
	if (ret) {
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

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte_config,

	SHELL_CMD_ARG(show, NULL,
	              "List current configuration.",
	              cmd_config_show, 1, 0),

	SHELL_CMD_ARG(test, NULL,
	              "Get/Set LTE test mode.",
	              cmd_config_test, 1, 1),

        SHELL_CMD_ARG(antenna, NULL,
	              "Get/Set LTE antenna (format: <int|ext>).",
	              cmd_config_antenna, 1, 1),

	SHELL_CMD_ARG(nb-iot-mode, NULL,
	              "Get/Set NB-IoT mode (format: <true|false>).",
	              cmd_config_nb_iot_mode, 1, 1),

	SHELL_CMD_ARG(lte-m-mode, NULL,
	              "Get/Set LTE-M mode (format: <true|false>).",
	              cmd_config_lte_m_mode, 1, 1),

	SHELL_CMD_ARG(autoconn, NULL,
	              "Get/Set auto-connect feature (format: <true|false>).",
	              cmd_config_autoconn, 1, 1),

	SHELL_CMD_ARG(plmnid, NULL,
	              "Get/Set network PLMN ID (format: <5-6 digits>).",
	              cmd_config_plmnid, 1, 1),

	SHELL_CMD_ARG(clksync, NULL,
	              "Get/Set clock synchronization (format: <true|false>).",
	              cmd_config_clksync, 1, 1),

	SHELL_CMD_ARG(apn, NULL,
	              "Get/Set network APN (format: <empty or up to 63 octets>.",
	              cmd_config_apn, 1, 1),

        SHELL_CMD_ARG(auth, NULL,
	              "Get/Set authentication protocol (format: <none|pap|chap>).",
	              cmd_config_auth, 1, 1),

	SHELL_CMD_ARG(username, NULL,
	              "Get/Set username (format: <empty or up to 32 characters>.",
	              cmd_config_username, 1, 1),

	SHELL_CMD_ARG(password, NULL,
	              "Get/Set password (format: <empty or up to 32 characters>.",
	              cmd_config_password, 1, 1),

	SHELL_CMD_ARG(addr, NULL,
	              "Get/Set default IP address (format: a.b.c.d).",
	              cmd_config_addr, 1, 1),

	SHELL_CMD_ARG(port, NULL,
	              "Get/Set default UDP port (format: <1-5 digits>).",
	              cmd_config_port, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte_test,

	SHELL_CMD_ARG(uart, NULL,
	              "Enable/Disable UART interface (format: <enable|disable>).",
	              cmd_test_uart, 2, 0),

	SHELL_CMD_ARG(reset, NULL,
	              "Reset modem.",
	              cmd_test_reset, 1, 0),

	SHELL_CMD_ARG(wakeup, NULL,
	              "Wake up modem.",
	              cmd_test_wakeup, 1, 0),

	SHELL_CMD_ARG(cmd, NULL,
	              "Send command to modem. (format: <command>)",
	              cmd_test_cmd, 2, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_lte,

	SHELL_CMD_ARG(config, &sub_lte_config,
	              "Configuration commands.",
	              print_help, 1, 0),

	SHELL_CMD_ARG(start, NULL,
	              "Start interface.", cmd_start, 1, 0),

	SHELL_CMD_ARG(attach, NULL,
	              "Attach to network.",
	              cmd_attach, 1, 0),

	SHELL_CMD_ARG(imei, NULL,
	              "Get modem IMEI.",
	              cmd_imei, 1, 0),

	SHELL_CMD_ARG(imsi, NULL,
	              "Get SIM card IMSI.",
	              cmd_imsi, 1, 0),

	SHELL_CMD_ARG(state, NULL,
	              "Get LTE state.",
	              cmd_state, 1, 0),

	SHELL_CMD_ARG(test, &sub_lte_test,
	              "Test commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(lte, &sub_lte, "LTE commands.", print_help);

/* clang-format on */

static int init(void)
{
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

	ret = ctr_lte_talk_init(talk_handler);
	if (ret) {
		LOG_ERR("Call `ctr_lte_talk_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_if_reset(dev_lte_if);
	if (ret) {
		LOG_ERR("Call `ctr_lte_if_reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
