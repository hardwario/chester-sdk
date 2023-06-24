/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "minmea.h"

/* CHESTER includes */
#include <chester/ctr_gnss.h>
#include <chester/drivers/m8.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define K_MSGQ_DEFINE_STATIC(q_name, q_msg_size, q_max_msgs, q_align)                              \
	static char __noinit __aligned(q_align) _k_fifo_buf_##q_name[(q_max_msgs) * (q_msg_size)]; \
	static STRUCT_SECTION_ITERABLE(k_msgq, q_name) =                                           \
		Z_MSGQ_INITIALIZER(q_name, _k_fifo_buf_##q_name, q_msg_size, q_max_msgs)

LOG_MODULE_REGISTER(ctr_gnss, CONFIG_CTR_GNSS_LOG_LEVEL);

#define CMD_MSGQ_MAX_ITEMS 16

enum cmd_msgq_req {
	CMD_MSGQ_REQ_START = 0,
	CMD_MSGQ_REQ_STOP = 1,
};

struct cmd_msgq_data_stop {
	bool keep_bckp_domain;
};

union cmd_msgq_data {
	struct cmd_msgq_data_stop stop;
};

struct cmd_msgq_item {
	int corr_id;
	enum cmd_msgq_req req;
	union cmd_msgq_data data;
};

static const struct device *m_m8_dev = DEVICE_DT_GET(DT_NODELABEL(m8));
static ctr_gnss_user_cb m_user_cb;
static void *m_user_data;
static atomic_t m_corr_id;
static bool m_running;
static char m_line_buf[256];
static size_t m_line_len;
static bool m_line_clipped;

K_MSGQ_DEFINE_STATIC(m_cmd_msgq, sizeof(struct cmd_msgq_item), CMD_MSGQ_MAX_ITEMS, 4);

static int process_req_start(const struct cmd_msgq_item *item)
{
	int ret;

	if (!device_is_ready(m_m8_dev)) {
		LOG_ERR("Device `M8` not ready");
		return -EINVAL;
	}

	ret = m8_set_main_power(m_m8_dev, true);
	if (ret) {
		LOG_ERR("Call `m8_set_main_power` failed: %d", ret);
		return -EIO;
	}

	ret = m8_set_bckp_power(m_m8_dev, true);
	if (ret) {
		LOG_ERR("Call `m8_set_bckp_power` failed: %d", ret);
		return -EIO;
	}

	m_line_len = 0;
	m_line_clipped = false;

	k_sleep(K_SECONDS(1));

	return 0;
}

static int process_req_stop(const struct cmd_msgq_item *item)
{
	int ret;

	if (!device_is_ready(m_m8_dev)) {
		LOG_ERR("Device `M8` not ready");
		return -EINVAL;
	}

	ret = m8_set_main_power(m_m8_dev, false);
	if (ret) {
		LOG_ERR("Call `m8_set_main_power` failed: %d", ret);
		return -EIO;
	}

	if (!item->data.stop.keep_bckp_domain) {
		ret = m8_set_bckp_power(m_m8_dev, false);
		if (ret) {
			LOG_ERR("Call `m8_set_bckp_power` failed: %d", ret);
			return -EIO;
		}
	}

	k_sleep(K_SECONDS(1));

	return 0;
}

static void process_cmd_msgq(void)
{
	int ret;

	struct cmd_msgq_item item;

	ret = k_msgq_get(&m_cmd_msgq, &item, m_running ? K_MSEC(100) : K_FOREVER);
	if (ret == -EAGAIN) {
		return;

	} else if (ret) {
		LOG_ERR("Call `k_msgq_get` failed: %d", ret);
		k_sleep(K_SECONDS(1));
		return;
	}

	if (item.req == CMD_MSGQ_REQ_START) {
		LOG_INF("Dequeued START command (correlation id: %d)", item.corr_id);

		if (m_running) {
			LOG_WRN("No reason for START operation - ignoring");

		} else {
			ret = process_req_start(&item);

			union ctr_gnss_event_data data = {0};

			if (ret) {
				LOG_ERR("Call `process_req_start` failed: %d", ret);

				if (m_user_cb) {
					data.start_err.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_START_ERR, &data, m_user_data);
				}

			} else {
				m_running = true;

				if (m_user_cb) {
					data.start_ok.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_START_OK, &data, m_user_data);
				}
			}
		}

	} else if (item.req == CMD_MSGQ_REQ_STOP) {
		if (!m_running) {
			LOG_WRN("No reason for STOP operation - ignoring");

		} else {
			ret = process_req_stop(&item);

			union ctr_gnss_event_data data = {0};

			if (ret) {
				LOG_ERR("Call `process_req_stop` failed: %d", ret);

				if (m_user_cb) {
					data.stop_err.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_STOP_ERR, &data, m_user_data);
				}

			} else {
				m_running = false;

				if (m_user_cb) {
					data.stop_ok.corr_id = item.corr_id;

					m_user_cb(CTR_GNSS_EVENT_STOP_OK, &data, m_user_data);
				}
			}
		}
	}
}

static void parse_nmea(const char *line)
{
	enum minmea_sentence_id sentence_id = minmea_sentence_id(line, true);

	if (sentence_id == MINMEA_SENTENCE_GGA) {
		struct minmea_sentence_gga frame;

		if (minmea_parse_gga(&frame, line)) {
			union ctr_gnss_event_data data = {0};

			data.update.fix_quality = frame.fix_quality;
			data.update.satellites_tracked = frame.satellites_tracked;
			data.update.latitude = minmea_tocoord(&frame.latitude);
			data.update.longitude = minmea_tocoord(&frame.longitude);
			data.update.altitude = minmea_tofloat(&frame.altitude);

			LOG_DBG("Fix quality: %d", data.update.fix_quality);
			LOG_DBG("Satellites tracked: %d", data.update.satellites_tracked);
			LOG_DBG("Latitude: %.7f", data.update.latitude);
			LOG_DBG("Longitude: %.7f", data.update.longitude);
			LOG_DBG("Altitude: %.1f", data.update.altitude);

			if (m_user_cb) {
				m_user_cb(CTR_GNSS_EVENT_UPDATE, &data, m_user_data);
			}
		}
	}
}

static void ingest_nmea(uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		char c = buf[i];

		if (c == '\r' || c == '\n') {
			if (m_line_len > 0) {
				if (!m_line_clipped) {
					LOG_DBG("Read line: %s", m_line_buf);
					parse_nmea(m_line_buf);

				} else {
					LOG_WRN("Line was clipped (ignoring)");
				}

				m_line_len = 0;
				m_line_clipped = false;
			}

		} else {
			if (m_line_len < sizeof(m_line_buf) - 2) {
				m_line_buf[m_line_len++] = c;
				m_line_buf[m_line_len + 1] = '\0';

			} else {
				m_line_clipped = true;
			}
		}
	}
}

static void dispatcher_thread(void)
{
	int ret;

	for (;;) {
		if (m_running && device_is_ready(m_m8_dev)) {
			uint8_t buf[64];
			size_t bytes_read;

			for (;;) {
				ret = m8_read_buffer(m_m8_dev, buf, sizeof(buf), &bytes_read);
				if (ret) {
					LOG_ERR("Call `m8_read_buffer` failed: %d", ret);

					if (m_user_cb) {
						m_user_cb(CTR_GNSS_EVENT_FAILURE, NULL,
							  m_user_data);
					}
				}

				if (!bytes_read) {
					break;
				}

				ingest_nmea(buf, bytes_read);

				k_sleep(K_MSEC(20));
			}
		}

		process_cmd_msgq();
	}
}

K_THREAD_DEFINE(ctr_gnss, CONFIG_CTR_GNSS_THREAD_STACK_SIZE, dispatcher_thread, NULL, NULL, NULL,
		CONFIG_CTR_GNSS_THREAD_PRIORITY, 0, 0);

int ctr_gnss_set_handler(ctr_gnss_user_cb user_cb, void *user_data)
{
	m_user_cb = user_cb;
	m_user_data = user_data;

	return 0;
}

int ctr_gnss_start(int *corr_id)
{
	int ret;

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_START,
	};

	LOG_INF("Enqueing START command (correlation id: %d)", item.corr_id);

	if (corr_id) {
		*corr_id = item.corr_id;
	}

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_gnss_stop(bool keep_bckp_domain, int *corr_id)
{
	int ret;

	struct cmd_msgq_item item = {
		.corr_id = (int)atomic_inc(&m_corr_id),
		.req = CMD_MSGQ_REQ_STOP,
		.data.stop.keep_bckp_domain = keep_bckp_domain,
	};

	LOG_INF("Enqueing STOP command (correlation id: %d)", item.corr_id);

	if (corr_id) {
		*corr_id = item.corr_id;
	}

	ret = k_msgq_put(&m_cmd_msgq, &item, K_NO_WAIT);
	if (ret) {
		LOG_ERR("Call `k_msgq_put` failed: %d", ret);
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
	ret = ctr_gnss_start(&corr_id);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_start` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "correlation id: %d", corr_id);

	return 0;
}

static int cmd_stop(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	int corr_id;
	ret = ctr_gnss_stop(false, &corr_id);
	if (ret) {
		LOG_ERR("Call `ctr_gnss_stop` failed: %d", ret);
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

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gnss,

	SHELL_CMD_ARG(start, NULL,
	              "Start receiver.",
	              cmd_start, 1, 0),

	SHELL_CMD_ARG(stop, NULL,
	              "Stop receiver.",
	              cmd_stop, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gnss, &sub_gnss, "GNSS commands.", print_help);

/* clang-format on */
