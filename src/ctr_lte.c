#include <ctr_lte.h>
#include <ctr_bsp.h>
#include <ctr_config.h>
#include <ctr_lte_talk.h>

/* Zephyr includes */
#include <init.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte, LOG_LEVEL_DBG);

#define SETTINGS_PFX "lte"

#define BOOT_RETRY_COUNT 3
#define BOOT_RETRY_DELAY K_SECONDS(10)
#define SETUP_RETRY_COUNT 1
#define SETUP_RETRY_DELAY K_SECONDS(10)
#define ATTACH_RETRY_COUNT 3
#define ATTACH_RETRY_DELAY K_SECONDS(60)

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
	enum antenna antenna;
};

static enum state m_state = STATE_INIT;

static struct config m_config_interim = {
	.antenna = ANTENNA_INT,
};

static struct config m_config;

static struct k_poll_signal m_boot_sig;
static struct k_poll_signal m_sim_card_sig;

K_MSGQ_DEFINE(m_cmd_msgq, sizeof(struct cmd_msgq_item), CMD_MSGQ_MAX_ITEMS, 4);
K_MSGQ_DEFINE(m_send_msgq, sizeof(struct send_msgq_item), SEND_MSGQ_MAX_ITEMS, 4);

static ctr_lte_event_cb m_event_cb;
static void *m_event_user_data;
static atomic_t m_corr_id;

static K_MUTEX_DEFINE(m_mut);
static uint64_t m_imsi;

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

	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

static int boot_once(void)
{
	int ret;

	ctr_bsp_set_lte_reset(0);
	k_sleep(K_MSEC(100));
	ctr_bsp_set_lte_reset(1);

	k_poll_signal_reset(&m_boot_sig);
	k_sleep(K_MSEC(1000));

	ctr_bsp_set_lte_wkup(0);
	k_sleep(K_MSEC(100));
	ctr_bsp_set_lte_wkup(1);

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

	ret = ctr_lte_talk_at_xtime(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_xtime` failed: %d", ret);
		return ret;
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

	return 0;
}

static int setup(int retries, k_timeout_t delay)
{
	int ret;

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

	k_poll_signal_reset(&m_sim_card_sig);

	ret = ctr_lte_talk_at_cfun(1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
		                         &m_sim_card_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_SECONDS(10));

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
		LOG_ERR("Call `ctr_lte_talk_at_cfun` failed: %d", ret);
		return ret;
	}

	LOG_INF("IMSI: %s", imsi);

	k_mutex_lock(&m_mut, K_FOREVER);
	m_imsi = strtoull(imsi, NULL, 10);
	k_mutex_unlock(&m_mut);

	k_sleep(K_SECONDS(30));

	return 0;
}

static int attach(int retries, k_timeout_t delay)
{
	int ret;

	LOG_INF("Operation ATTACH started");

	while (retries-- > 0) {
		ret = ctr_lte_talk_enable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_enable` failed: %d", ret);
			return ret;
		}

		ret = attach_once();

		if (ret == 0) {
			ret = ctr_lte_talk_disable();

			if (ret < 0) {
				LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
				return ret;
			}

			LOG_INF("Operation ATTACH succeeded");
			return 0;
		}

		if (ret < 0) {
			LOG_WRN("Call `join_once` failed: %d", ret);
		}

		ret = ctr_lte_talk_disable();

		if (ret < 0) {
			LOG_ERR("Call `ctr_lte_talk_disable` failed: %d", ret);
			return ret;
		}

		if (retries > 0) {
			k_sleep(delay);
			LOG_WRN("Repeating ATTACH operation (retries left: %d)", retries);
		}
	}

	LOG_WRN("Operation ATTACH failed");
	return -ENOTCONN;
}

static int start(void)
{
	int ret;

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

	// TODO
	// ret = process_req_send(&item);
	ret = 0;

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

	SETTINGS_SET("antenna", &m_config_interim.antenna, sizeof(m_config_interim.antenna));

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

	EXPORT_FUNC("antenna", &m_config_interim.antenna, sizeof(m_config_interim.antenna));

#undef EXPORT_FUNC

	return 0;
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

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_antenna(shell);

	return 0;
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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lte_config,
                               SHELL_CMD_ARG(show, NULL, "List current configuration.",
                                             cmd_config_show, 1, 0),
                               SHELL_CMD_ARG(antenna, NULL,
                                             "Get/Set LoRaWAN antenna (format: <int|ext>).",
                                             cmd_config_antenna, 1, 1),
                               SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_lte,
                               SHELL_CMD_ARG(config, &sub_lte_config, "Configuration commands.",
                                             print_help, 1, 0),
                               SHELL_CMD_ARG(start, NULL, "Start interface.", cmd_start, 1, 0),
                               SHELL_CMD_ARG(attach, NULL, "Attach to network.", cmd_attach, 1, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(lte, &sub_lte, "LTE commands.", print_help);

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	k_poll_signal_init(&m_boot_sig);
	k_poll_signal_init(&m_sim_card_sig);

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

	if (m_config.antenna == ANTENNA_EXT) {
		ret = ctr_bsp_set_rf_ant(CTR_BSP_RF_ANT_EXT);
	} else {
		ret = ctr_bsp_set_rf_ant(CTR_BSP_RF_ANT_INT);
	}

	if (ret < 0) {
		LOG_ERR("Call `ctr_bsp_set_rf_ant` failed: %d", ret);
		return ret;
	}

	ret = ctr_bsp_set_rf_mux(CTR_BSP_RF_MUX_LTE);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bsp_set_rf_mux` failed: %d", ret);
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

#if 0
#include <ctr_lte.h>
#include <ctr_bsp.h>
#include <ctr_lte_talk.h>
#include <ctr_lte_tok.h>
#include <ctr_lte_uart.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_lte, LOG_LEVEL_DBG);

#define SEND_MSGQ_MAX_ITEMS 16
#define RECV_MSGQ_MAX_ITEMS 16

#define SEND_MAX_LEN 512

#define TIMEOUT_S CTR_SYS_MSEC(5000)
#define TIMEOUT_L CTR_SYS_SECONDS(30)

typedef struct {
	int64_t ttl;
	uint8_t addr[4];
	int port;
	const void *buf;
	size_t len;
} send_item_t;

typedef struct {
	int64_t toa;
	int port;
	void *buf;
	size_t len;
} recv_item_t;

typedef enum {
	STATE_RESET = 0,
	STATE_ATTACHED = 1,
	STATE_DETACHED = 2,
} state_t;

typedef struct {
	const ctr_lte_cfg_t *cfg;
	ctr_lte_callback_t cb;
	void *param;
	ctr_sys_mut_t mut;
	ctr_sys_sem_t sem;
	ctr_sys_msgq_t send_msgq;
	char __aligned(4) send_msgq_mem[SEND_MSGQ_MAX_ITEMS * sizeof(send_item_t)];
	ctr_sys_msgq_t recv_msgq;
	char __aligned(4) recv_msgq_mem[RECV_MSGQ_MAX_ITEMS * sizeof(recv_item_t)];
	ctr_sys_task_t task;
	CTR_SYS_TASK_STACK_MEMBER(stack, 4096);
	state_t state_now;
	state_t state_req;
	bool registered;
	char buf[2 * SEND_MAX_LEN + 1];
} ctr_lte_t;

static ctr_lte_t inst;

static int sleep(void)
{
#if 1
	ctr_lte_t *ctx = &inst;

	int ret = 0;

	LOG_INF("Sleep initiated [%p]", ctx);

	/* TODO Unfortunately, we are not getting acknowledgement to this, but the */
	/* ticket has been raised */
	if (ctr_lte_talk_cmd("AT#XSLEEP=2") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		ret = -1;
	}

	/* TODO Why needed? */
	ctr_sys_task_sleep(CTR_SYS_SECONDS(2));
#endif

	if (ctr_lte_uart_disable()) {
		LOG_ERR("Call `ctr_lte_uart_disable` failed [%p]", ctx);
		ret = -2;
	}

	return ret;
}

static int wake_up(void)
{
#if 1
	ctr_lte_t *ctx = &inst;

	LOG_INF("Wake up initiated [%p]", ctx);

	if (ctr_lte_uart_enable() < 0) {
		LOG_ERR("Call `ctr_lte_uart_enable` failed [%p]", ctx);
		return -1;
	}

	if (ctr_bsp_set_lte_wkup(0) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_wkup` failed [%p]", ctx);
		return -2;
	}

	ctr_sys_task_sleep(CTR_SYS_MSEC(10));

	if (ctr_bsp_set_lte_wkup(1) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_wkup` failed [%p]", ctx);
		return -3;
	}

	/* TODO Why needed? */
	ctr_sys_task_sleep(CTR_SYS_SECONDS(1));
#endif

	return 0;
}

static int attach_once(void)
{
	ctr_lte_t *ctx = &inst;

	if (ctr_bsp_set_lte_reset(0) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_reset` failed [%p]", ctx);
		return -1;
	}

	ctr_sys_task_sleep(CTR_SYS_MSEC(10));

	if (ctr_bsp_set_lte_reset(1) < 0) {
		LOG_ERR("Call `ctr_bsp_set_lte_reset` failed [%p]", ctx);
		return -2;
	}

	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));

	if (wake_up() < 0) {
		LOG_ERR("Call `wake_up` failed [%p]", ctx);
		return -3;
	}

	char *rsp;

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%HWVERSION") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -4;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%SHORTSWVER") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -5;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XPOFWARN=1,30") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -6;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSYSTEMMODE=0,1,0,0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -7;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%REL14FEAT=1,1,1,1,0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -8;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XDATAPRFL=0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -9;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XSIM=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -10;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XNETTIME=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -11;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT%%XTIME=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -12;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEREG=5") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -13;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CGEREP=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -14;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CSCON=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -15;
	}

	const char *timer_t3412 = "00111000"; /* T3412 Extended Timer */
	const char *timer_t3324 = "00000000"; /* T3324 Active Timer */

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CPSMS=1,,,\"%s\",\"%s\"", timer_t3412, timer_t3324) <
	    0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -16;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CNEC=24") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -17;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CMEE=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -18;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CEPPI=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -19;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=1") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -20;
	}

	/* TODO Remove */
	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));

	if (ctr_lte_talk_cmd("AT+CIMI") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -21;
	}

	if (ctr_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_rsp` failed [%p]", ctx);
		return -22;
	}

	if (ctr_lte_talk_ok(TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_ok` failed [%p]", ctx);
		return -23;
	}

	int64_t end = ctr_sys_uptime_get() + CTR_SYS_MINUTES(5);

	for (;;) {
		int64_t now = ctr_sys_uptime_get();

		if (now >= end) {
			LOG_WRN("Attach timed out [%p]", ctx);
			return -24;
		}

		if (ctr_sys_sem_take(&ctx->sem, end - now) < 0) {
			continue;
		}

		ctr_sys_mut_acquire(&ctx->mut);
		bool registered = ctx->registered;
		ctr_sys_mut_release(&ctx->mut);

		if (registered) {
			break;
		}
	}

	if (ctr_lte_talk_cmd("AT#XSOCKET=1,2,0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		return -25;
	}

	/* TODO Short timeout? */
	if (ctr_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_rsp` failed [%p]", ctx);
		return -26;
	}

	if (strcmp(rsp, "#XSOCKET: 1,2,0,17") != 0) {
		LOG_ERR("Unexpected response [%p]", ctx);
		return -27;
	}

	if (ctr_lte_talk_ok(TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_ok` failed [%p]", ctx);
		return -28;
	}

	if (sleep() < 0) {
		return -29;
	}

	/* TODO Disable UART */

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

static int attach(void)
{
	ctr_lte_t *ctx = &inst;

	LOG_INF("Attach started [%p]", ctx);

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->registered = false;
	int retries = ctx->cfg->attach_retries;
	int64_t pause = ctx->cfg->attach_pause;
	ctr_sys_mut_release(&ctx->mut);

	if (retries <= 0) {
		LOG_ERR("Parameter `retries` invalid [%p]", ctx);
		return -1;
	}

	do {
		if (attach_once() < 0) {
			goto error;
		}

		LOG_INF("Attach succeeded [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_lte_event_t event = { 0 };
			event.source = CTR_LTE_EVENT_ATTACH_DONE;
			ctx->cb(&event, ctx->param);
		}

		return 0;

	error:
		LOG_WRN("Attach failed [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_lte_event_t event = { 0 };
			event.source = CTR_LTE_EVENT_ATTACH_ERROR;
			ctx->cb(&event, ctx->param);
			if (event.opts.attach_error.stop) {
				LOG_INF("Attach cancelled [%p]", ctx);
				return -2;
			}
		}

		ctr_sys_task_sleep(pause);

	} while (--retries > 0);

	return -3;
}

static int detach(void)
{
	ctr_lte_t *ctx = &inst;

	LOG_INF("Detach started [%p]", ctx);

	int ret = 0;

	if (wake_up() < 0) {
		LOG_ERR("Call `wake_up` failed [%p]", ctx);
		ret = -1;
		goto error;
	}

	if (ctr_lte_talk_cmd_ok(TIMEOUT_S, "AT+CFUN=0") < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd_ok` failed [%p]", ctx);
		ret = -2;
		goto error;
	}

	if (sleep() < 0) {
		LOG_ERR("Call `sleep` failed [%p]", ctx);
		ret = -3;
		goto error;
	}

	LOG_INF("Detach succeeded [%p]", ctx);

	if (ctx->cb != NULL) {
		ctr_lte_event_t event = { 0 };
		event.source = CTR_LTE_EVENT_DETACH_DONE;
		ctx->cb(&event, ctx->param);
	}

	return 0;

error:
	LOG_WRN("Detach failed [%p]", ctx);

	if (ctx->cb != NULL) {
		ctr_lte_event_t event = { 0 };
		event.source = CTR_LTE_EVENT_DETACH_ERROR;
		ctx->cb(&event, ctx->param);
	}

	return ret;
}

static state_t check_state(void)
{
	ctr_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	state_t state_now = ctx->state_now;
	state_t state_req = ctx->state_req;
	ctr_sys_mut_release(&ctx->mut);

	if (state_now != state_req) {
		switch (state_req) {
		case STATE_ATTACHED:
			if (attach() < 0) {
				LOG_WRN("Call `attach` failed [%p]", ctx);
			} else {
				state_now = STATE_ATTACHED;

				ctr_sys_mut_acquire(&ctx->mut);
				ctx->state_now = state_now;
				ctr_sys_mut_release(&ctx->mut);
			}
			break;
		case STATE_DETACHED:
			if (detach() < 0) {
				LOG_WRN("Call `detach` failed [%p]", ctx);
			} else {
				state_now = STATE_DETACHED;

				ctr_sys_mut_acquire(&ctx->mut);
				ctx->state_now = state_now;
				ctr_sys_mut_release(&ctx->mut);
			}
			break;
		default:
			LOG_ERR("Invalid state requested [%p]", ctx);
			break;
		}
	}

	return state_now;
}

static int hexify(char *dst, size_t size, const void *src, size_t len)
{
	if (size < len * 2 + 1) {
		return -ENOBUFS;
	}

	memset(dst, 0, size);

	for (uint8_t *p = (uint8_t *)src; len != 0; p++, len--) {
		*dst++ = (*p >> 4) >= 10 ? (*p >> 4) - 10 + 'A' : (*p >> 4) + '0';
		*dst++ = (*p & 15) >= 10 ? (*p & 15) - 10 + 'A' : (*p & 15) + '0';
	}

	return 0;
}

static int send_once(send_item_t *item)
{
	ctr_lte_t *ctx = &inst;

	if (hexify(ctx->buf, sizeof(ctx->buf), item->buf, item->len) < 0) {
		LOG_ERR("Call `hexify` failed [%p]", ctx);
		return -1;
	}

	if (wake_up() < 0) {
		LOG_ERR("Call `wake_up` failed [%p]", ctx);
		return -2;
	}

	char *rsp;

	if (ctr_lte_talk_cmd("AT#XSENDTO=\"%u.%u.%u.%u\",%u,0,\"%s\"", item->addr[0], item->addr[1],
	                     item->addr[2], item->addr[3], item->port, ctx->buf) < 0) {
		LOG_ERR("Call `ctr_lte_talk_cmd` failed [%p]", ctx);
		return -3;
	}

	if (ctr_lte_talk_rsp(&rsp, TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_rsp` failed [%p]", ctx);
		return -4;
	}

	if ((rsp = ctr_lte_tok_pfx(rsp, "#XSENDTO: ")) == NULL) {
		LOG_ERR("Call `ctr_lte_tok_pfx` failed [%p]", ctx);
		return -5;
	}

	bool def;
	long num;

	if ((rsp = ctr_lte_tok_num(rsp, &def, &num)) == NULL) {
		LOG_ERR("Call `ctr_lte_tok_num` failed [%p]", ctx);
		return -6;
	}

	if (!def || num != item->len) {
		LOG_ERR("Number of sent bytes does not match [%p]", ctx);
		return -7;
	}

	if ((rsp = ctr_lte_tok_end(rsp)) == NULL) {
		LOG_ERR("Call `ctr_lte_tok_end` failed [%p]", ctx);
		return -8;
	}

	if (ctr_lte_talk_ok(TIMEOUT_S) < 0) {
		LOG_ERR("Call `ctr_lte_talk_ok` failed [%p]", ctx);
		return -9;
	}

	if (sleep() < 0) {
		LOG_ERR("Call `sleep` failed [%p]", ctx);
		return -10;
	}

	return 0;
}

static int send(send_item_t *item)
{
	ctr_lte_t *ctx = &inst;

	LOG_INF("Send started [%p]", ctx);

	ctr_sys_mut_acquire(&ctx->mut);
	int retries = ctx->cfg->send_retries;
	int64_t pause = ctx->cfg->send_pause;
	ctr_sys_mut_release(&ctx->mut);

	if (retries <= 0) {
		LOG_ERR("Parameter `retries` invalid [%p]", ctx);
		return -1;
	}

	if (ctr_sys_uptime_get() > item->ttl) {
		LOG_WRN("Message TTL expired [%p]", ctx);
		return -2;
	}

	do {
		if (send_once(item) < 0) {
			goto error;
		}

		LOG_INF("Send succeeded [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_lte_event_t event = { 0 };
			event.source = CTR_LTE_EVENT_SEND_DONE;
			event.info.send_done.buf = item->buf;
			event.info.send_done.len = item->len;
			ctx->cb(&event, ctx->param);
		}

		return 0;

	error:
		LOG_WRN("Send failed [%p]", ctx);

		if (ctx->cb != NULL) {
			ctr_lte_event_t event = { 0 };
			event.source = CTR_LTE_EVENT_SEND_ERROR;
			ctx->cb(&event, ctx->param);
			if (event.opts.send_error.stop) {
				LOG_INF("Send cancelled [%p]", ctx);
				return -3;
			}
		}

		if (attach() < 0) {
			LOG_WRN("Call `attach` failed");
			return -4;
		}

		ctr_sys_task_sleep(pause);

	} while (--retries > 0);

	return -5;
}

static void check_send(void)
{
	ctr_lte_t *ctx = &inst;

	for (;;) {
		send_item_t item;

		if (ctr_sys_msgq_get(&ctx->send_msgq, &item, CTR_SYS_NO_WAIT) < 0) {
			break;
		}

		LOG_INF("Dequeued message to send (port %d, len %u) [%p]", item.port, item.len,
		        ctx);

		if (send(&item) < 0) {
			LOG_ERR("Call `send` failed [%p]", ctx);
		}
	}
}

static void entry(void *param)
{
	ctr_lte_t *ctx = param;

	if (ctr_bsp_set_rf_ant(CTR_BSP_RF_ANT_INT) < 0) {
		LOG_ERR("Call `ctr_bsp_set_rf_ant` failed [%p]", ctx);
		return;
	}

	if (ctr_bsp_set_rf_mux(CTR_BSP_RF_MUX_LTE) < 0) {
		LOG_ERR("Call `ctr_bsp_set_rf_mux` failed [%p]", ctx);
		return;
	}

	if (ctr_lte_uart_init() < 0) {
		LOG_ERR("Call `ctr_lte_uart_init` failed [%p]", ctx);
		return;
	}

	for (;;) {
		if (ctr_sys_sem_take(&ctx->sem, CTR_SYS_FOREVER) < 0) {
			LOG_ERR("Call `ctr_sys_sem_take` failed [%p]", ctx);
			return;
		}

		state_t state = check_state();

		if (state == STATE_ATTACHED) {
			check_send();
		}
	}
}

int ctr_lte_init(const ctr_lte_cfg_t *cfg)
{
	ctr_lte_t *ctx = &inst;

	memset(ctx, 0, sizeof(*ctx));

	ctx->cfg = cfg;

	ctr_sys_mut_init(&ctx->mut);
	ctr_sys_sem_init(&ctx->sem, 0);
	ctr_sys_msgq_init(&ctx->send_msgq, ctx->send_msgq_mem, sizeof(send_item_t),
	                  SEND_MSGQ_MAX_ITEMS);
	ctr_sys_msgq_init(&ctx->recv_msgq, ctx->recv_msgq_mem, sizeof(recv_item_t),
	                  RECV_MSGQ_MAX_ITEMS);
	ctr_sys_task_init(&ctx->task, "ctr_lte", ctx->stack,
	                  CTR_SYS_TASK_STACK_SIZEOF(ctx->stack), entry, ctx);

	return 0;
}

int ctr_lte_set_callback(ctr_lte_callback_t cb, void *param)
{
	ctr_lte_t *ctx = &inst;

	ctx->cb = cb;
	ctx->param = param;

	return 0;
}

int ctr_lte_attach(void)
{
	ctr_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->state_req = STATE_ATTACHED;
	ctr_sys_mut_release(&ctx->mut);

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

int ctr_lte_detach(void)
{
	ctr_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->state_req = STATE_DETACHED;
	ctr_sys_mut_release(&ctx->mut);

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

int ctr_lte_send(const ctr_net_send_opts_t *opts, const void *buf, size_t len)
{
	ctr_lte_t *ctx = &inst;

	send_item_t item = {
		.ttl = opts->ttl,
		.addr = { opts->addr[0], opts->addr[1], opts->addr[2], opts->addr[3] },
		.port = opts->port,
		.buf = buf,
		.len = len,
	};

	if (ctr_sys_msgq_put(&ctx->send_msgq, &item, CTR_SYS_NO_WAIT) < 0) {
		LOG_ERR("Call `ctr_sys_msgq_put` failed [%p]", ctx);
		return -1;
	}

	ctr_sys_sem_give(&ctx->sem);

	return 0;
}

int ctr_lte_recv(ctr_net_recv_info_t *info, void *buf, size_t size)
{
	return 0;
}

void ctr_lte_set_reg(bool state)
{
	ctr_lte_t *ctx = &inst;

	ctr_sys_mut_acquire(&ctx->mut);
	ctx->registered = state;
	ctr_sys_mut_release(&ctx->mut);

	ctr_sys_sem_give(&ctx->sem);
}

/*

test start
test lte reset
test lte wakeup
test lte cmd AT%XSYSTEMMODE=0,1,0,0
test lte cmd AT%REL14FEAT=1,1,1,1,0
test lte cmd AT%XDATAPRFL=0
test lte cmd AT%XSIM=1
test lte cmd AT%XNETTIME=1
test lte cmd AT%XTIME=1
test lte cmd AT+CEREG=5
test lte cmd AT+CGEREP=1
test lte cmd AT+CSCON=1
test lte cmd 'AT+CPSMS=1,,,"00111000","00000000"'
test lte cmd AT+CNEC=24
test lte cmd AT+CMEE=1
test lte cmd AT+CEPPI=1
test lte cmd AT+CFUN=1



*/
#endif
