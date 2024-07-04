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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_gnss_m8, CONFIG_CTR_GNSS_LOG_LEVEL);

static const struct device *m_m8_dev = DEVICE_DT_GET(DT_NODELABEL(m8));
static char m_line_buf[256];
static size_t m_line_len;
static bool m_line_clipped;
static ctr_gnss_user_cb m_user_cb;
static void *m_user_data;

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

int ctr_gnss_m8_start(void)
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

int ctr_gnss_m8_stop(bool keep_bckp_domain)
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

	if (!keep_bckp_domain) {
		ret = m8_set_bckp_power(m_m8_dev, false);
		if (ret) {
			LOG_ERR("Call `m8_set_bckp_power` failed: %d", ret);
			return -EIO;
		}
	}

	k_sleep(K_SECONDS(1));

	return 0;
}

int ctr_gnss_m8_set_handler(ctr_gnss_user_cb user_cb, void *user_data)
{
	m_user_cb = user_cb;
	m_user_data = user_data;

	return 0;
}

int ctr_gnss_m8_process_data(void)
{
	if (!device_is_ready(m_m8_dev)) {
		LOG_ERR("Device `M8` not ready");
		return -EINVAL;
	}

	int ret;
	uint8_t buf[64];
	size_t bytes_read;

	for (;;) {
		ret = m8_read_buffer(m_m8_dev, buf, sizeof(buf), &bytes_read);
		if (ret) {
			LOG_ERR("Call `m8_read_buffer` failed: %d", ret);

			if (m_user_cb) {
				m_user_cb(CTR_GNSS_EVENT_FAILURE, NULL, m_user_data);
			}
		}

		if (!bytes_read) {
			break;
		}

		ingest_nmea(buf, bytes_read);

		k_sleep(K_MSEC(20));
	}

	return 0;
}
