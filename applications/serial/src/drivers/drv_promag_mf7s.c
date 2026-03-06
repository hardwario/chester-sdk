/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/**
 * @file drv_promag_mf7s.c
 * @brief Promag MF7S RFID reader driver implementation
 *
 * This driver communicates with Promag MF7S via RS-232 (19200, 8N1).
 * A background listener thread continuously receives card UIDs.
 * Card frame: STX(0x02) + 8 HEX chars (UID) + CR(0x0D) + LF(0x0A) + ETX(0x03)
 * Host command: STX(0x02) + COMMAND + CR(0x0D)
 */

#include "drv_promag_mf7s.h"
#include "drv_interface.h"
#include "../app_device.h"
#include "../app_config.h"
#include "../app_serial.h"
#include "../app_work.h"

#include <chester/ctr_led.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(drv_promag_mf7s, LOG_LEVEL_INF);

#define MAX_SAMPLES 32

/* MF7S protocol constants */
#define STX 0x02
#define ETX 0x03
#define CR  0x0D
#define LF  0x0A

/* Card frame: STX + 8 hex + CR + LF + ETX = 12 bytes */
#define CARD_FRAME_LEN 12

/* Frame buffer for accumulating received bytes */
#define FRAME_BUF_SIZE 64

/* Listener thread */
#define LISTENER_STACK_SIZE 1024
#define LISTENER_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO

/* Timeouts */
#define LISTENER_POLL_MS    1000
#define CMD_RESPONSE_MS     2000

static struct app_data_promag_mf7s m_data = {
	.last_uid = 0,
	.last_read_ts = 0,
	.total_reads = 0,
	.error_count = 0,
	.sampling_active = false,
	.valid = false,
};

static K_MUTEX_DEFINE(m_data_mutex);

/* Sample buffer for CBOR encoding */
static struct promag_mf7s_sample m_samples[MAX_SAMPLES];
static int m_sample_count;
static K_MUTEX_DEFINE(m_samples_mutex);

/* Listener thread control */
static atomic_t m_listener_enabled = ATOMIC_INIT(0);
static atomic_t m_listener_paused = ATOMIC_INIT(0);
static K_SEM_DEFINE(m_pause_sem, 0, 1);
static K_SEM_DEFINE(m_paused_ack, 0, 1);

/* Sampling mode state */
static atomic_t m_sampling_mode = ATOMIC_INIT(0);
static const struct shell *m_sampling_shell;
static int64_t m_sampling_idle_deadline;
static int m_sampling_timeout_s;

/* Forward declarations */
static void listener_thread_fn(void *, void *, void *);

K_THREAD_DEFINE(promag_listener, LISTENER_STACK_SIZE, listener_thread_fn,
		NULL, NULL, NULL, LISTENER_PRIORITY, 0, SYS_FOREVER_MS);

/**
 * @brief Parse hex character to nibble value
 */
static int hex_nibble(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}

/**
 * @brief Parse 8 hex characters into uint32_t UID
 */
static int parse_uid(const uint8_t *hex_chars, uint32_t *uid)
{
	uint32_t val = 0;

	for (int i = 0; i < 8; i++) {
		int nib = hex_nibble((char)hex_chars[i]);
		if (nib < 0) {
			return -EINVAL;
		}
		val = (val << 4) | nib;
	}

	*uid = val;
	return 0;
}

/**
 * @brief Parse card frame from buffer
 *
 * Looks for pattern: STX + 8 hex chars + CR + LF + ETX (12 bytes)
 *
 * @param buf Input buffer
 * @param len Buffer length
 * @param uid Output UID
 * @param consumed Number of bytes consumed (including frame)
 * @return 0 if frame found, -EAGAIN if incomplete, -EINVAL if invalid
 */
static int parse_card_frame(const uint8_t *buf, size_t len, uint32_t *uid, size_t *consumed)
{
	/* Search for STX in buffer */
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != STX) {
			continue;
		}

		/* Check if we have enough bytes for a complete frame */
		size_t remaining = len - i;
		if (remaining < CARD_FRAME_LEN) {
			/* Incomplete frame - consume bytes before STX */
			*consumed = i;
			return -EAGAIN;
		}

		/* Verify frame structure: STX + 8hex + CR + LF + ETX */
		const uint8_t *frame = &buf[i];
		if (frame[9] == CR && frame[10] == LF && frame[11] == ETX) {
			/* Verify all 8 chars are hex */
			bool valid_hex = true;
			for (int j = 1; j <= 8; j++) {
				if (hex_nibble((char)frame[j]) < 0) {
					valid_hex = false;
					break;
				}
			}

			if (valid_hex) {
				int ret = parse_uid(&frame[1], uid);
				if (ret == 0) {
					*consumed = i + CARD_FRAME_LEN;
					return 0;
				}
			}
		}

		/* Invalid frame at this STX - skip it */
	}

	/* No valid frame found, consume everything except potential partial frame */
	if (len > 0 && buf[len - 1] == STX) {
		*consumed = len - 1;
	} else {
		*consumed = len;
	}
	return -EAGAIN;
}

/**
 * @brief Send command to MF7S reader
 *
 * Frame: STX + command + CR
 */
static int send_mf7s_command(const char *cmd)
{
	size_t cmd_len = strlen(cmd);
	uint8_t frame[32];
	size_t idx = 0;

	if (cmd_len > sizeof(frame) - 2) {
		return -EINVAL;
	}

	frame[idx++] = STX;
	memcpy(&frame[idx], cmd, cmd_len);
	idx += cmd_len;
	frame[idx++] = CR;

	app_serial_flush_rx();

	int ret = app_serial_send(frame, idx);
	if (ret < 0) {
		LOG_ERR("Failed to send command '%s': %d", cmd, ret);
		return ret;
	}

	LOG_DBG("Sent command '%s' (%zu bytes)", cmd, idx);
	return 0;
}

/**
 * @brief Receive response from MF7S (text terminated by CR)
 */
static int receive_mf7s_response(char *out, size_t max_len, uint32_t timeout_ms)
{
	uint8_t buf[64];
	size_t buf_len = 0;

	int ret = app_serial_receive_timeout(buf, sizeof(buf), &buf_len, timeout_ms);
	if (ret) {
		return ret;
	}

	/* Find CR terminator and extract text */
	size_t text_len = 0;
	for (size_t i = 0; i < buf_len; i++) {
		if (buf[i] == CR || buf[i] == LF || buf[i] == ETX) {
			break;
		}
		if (buf[i] == STX) {
			continue;
		}
		if (text_len < max_len - 1) {
			out[text_len++] = (char)buf[i];
		}
	}

	out[text_len] = '\0';
	return (text_len > 0) ? 0 : -EIO;
}

/**
 * @brief Pause listener thread for exclusive UART access
 */
static void listener_pause(void)
{
	if (!atomic_get(&m_listener_enabled)) {
		return;
	}

	atomic_set(&m_listener_paused, 1);
	/* Wait for listener to acknowledge pause */
	k_sem_take(&m_paused_ack, K_MSEC(2000));
}

/**
 * @brief Resume listener thread
 */
static void listener_resume(void)
{
	atomic_set(&m_listener_paused, 0);
	k_sem_give(&m_pause_sem);
}

/**
 * @brief Store card UID and trigger send if needed
 */
static void store_card(uint32_t uid)
{
	/* Visual indication — orange LED for 1s */
	ctr_led_set(CTR_LED_CHANNEL_Y, true);

	/* Update data */
	uint64_t ts;
	ctr_rtc_get_ts(&ts);

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.last_uid = uid;
	m_data.last_read_ts = (uint32_t)ts;
	m_data.total_reads++;
	m_data.valid = true;
	k_mutex_unlock(&m_data_mutex);

	/* Add to sample buffer */
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	if (m_sample_count < MAX_SAMPLES) {
		uint64_t ts;
		ctr_rtc_get_ts(&ts);

		m_samples[m_sample_count].timestamp = ts;
		m_samples[m_sample_count].uid = uid;
		m_sample_count++;
		LOG_DBG("Promag: Added sample %d (UID=%08X)", m_sample_count, uid);
	} else {
		LOG_WRN("Promag: Sample buffer full");
	}
	k_mutex_unlock(&m_samples_mutex);

	/* Trigger send based on mode */
	if (g_app_config.mode == APP_CONFIG_MODE_LRW) {
		app_work_send();
	} else if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		if (g_app_config.interval_report == 0) {
			app_work_send();
		}
		/* interval_report > 0: samples accumulate, sent at periodic report */
	}

	/* LED off after 1s */
	k_msleep(1000);
	ctr_led_set(CTR_LED_CHANNEL_Y, false);
}

/**
 * @brief Background listener thread
 *
 * Continuously polls UART for card frames from the MF7S reader.
 */
static void listener_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t frame_buf[FRAME_BUF_SIZE];
	size_t frame_buf_len = 0;

	LOG_INF("Listener thread started, waiting for enable...");

	/* Wait until enabled */
	while (!atomic_get(&m_listener_enabled)) {
		k_sleep(K_MSEC(100));
	}

	LOG_INF("Listener thread active");

	while (true) {
		/* Check for pause request */
		if (atomic_get(&m_listener_paused)) {
			k_sem_give(&m_paused_ack);
			k_sem_take(&m_pause_sem, K_FOREVER);
			/* Flush any stale data after resume */
			frame_buf_len = 0;
			app_serial_flush_rx();
			continue;
		}

		/* Poll UART */
		uint8_t rx_buf[32];
		size_t rx_len = 0;
		int ret = app_serial_receive_timeout(rx_buf, sizeof(rx_buf), &rx_len,
						     LISTENER_POLL_MS);

		if (ret || rx_len == 0) {
			continue;
		}

		/* Accumulate into frame buffer */
		size_t copy_len = rx_len;
		if (frame_buf_len + copy_len > FRAME_BUF_SIZE) {
			copy_len = FRAME_BUF_SIZE - frame_buf_len;
		}
		memcpy(&frame_buf[frame_buf_len], rx_buf, copy_len);
		frame_buf_len += copy_len;

		/* Try to parse frames */
		while (frame_buf_len > 0) {
			uint32_t uid;
			size_t consumed = 0;

			ret = parse_card_frame(frame_buf, frame_buf_len, &uid, &consumed);

			if (ret == 0) {
				/* Card detected */
				LOG_INF("Card detected: UID=%08X (dec=%u)", uid, uid);

				/* Sampling mode - only print to shell, no data storage */
				if (atomic_get(&m_sampling_mode) && m_sampling_shell) {
					shell_print(m_sampling_shell,
						    "Card UID: %08X (dec: %u)", uid, uid);

					/* Reset idle timeout */
					m_sampling_idle_deadline =
						k_uptime_get() +
						(m_sampling_timeout_s * 1000);
				} else {
					store_card(uid);
				}

				/* Compact buffer */
				if (consumed < frame_buf_len) {
					memmove(frame_buf, &frame_buf[consumed],
						frame_buf_len - consumed);
				}
				frame_buf_len -= consumed;
			} else {
				/* No complete frame - compact consumed bytes */
				if (consumed > 0 && consumed < frame_buf_len) {
					memmove(frame_buf, &frame_buf[consumed],
						frame_buf_len - consumed);
					frame_buf_len -= consumed;
				} else if (consumed >= frame_buf_len) {
					frame_buf_len = 0;
				}
				break;
			}
		}

		/* Prevent buffer overflow */
		if (frame_buf_len >= FRAME_BUF_SIZE) {
			LOG_WRN("Frame buffer overflow, resetting");
			frame_buf_len = 0;
		}
	}
}

static int init(void)
{
	LOG_INF("Initializing Promag MF7S driver");

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.last_uid = 0;
	m_data.last_read_ts = 0;
	m_data.total_reads = 0;
	m_data.error_count = 0;
	m_data.sampling_active = false;
	m_data.valid = false;
	k_mutex_unlock(&m_data_mutex);

	/* Enable and start listener thread */
	atomic_set(&m_listener_enabled, 1);
	k_thread_start(promag_listener);

	LOG_INF("Promag MF7S driver initialized");

	return 0;
}

static int sample(void)
{
	/* MF7S is event-driven (auto-sends UID on card present).
	 * Background listener thread handles card detection.
	 * No periodic polling needed - use shell command 'read_info' to check reader.
	 */
	return 0;
}

const struct app_data_promag_mf7s *promag_mf7s_get_data(void)
{
	return &m_data;
}

int promag_mf7s_get_samples(struct promag_mf7s_sample *out, int max_count)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = (m_sample_count < max_count) ? m_sample_count : max_count;
	for (int i = 0; i < count; i++) {
		out[i] = m_samples[i];
	}
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

void promag_mf7s_clear_samples(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	m_sample_count = 0;
	k_mutex_unlock(&m_samples_mutex);
	LOG_DBG("Promag: Sample buffer cleared");
}

int promag_mf7s_get_sample_count(void)
{
	k_mutex_lock(&m_samples_mutex, K_FOREVER);
	int count = m_sample_count;
	k_mutex_unlock(&m_samples_mutex);
	return count;
}

static void print_data(const struct shell *shell, int idx, int addr)
{
	const struct app_data_promag_mf7s *d = promag_mf7s_get_data();
	shell_print(shell, "[%d] Promag MF7S RFID:", idx);
	shell_print(shell, "  Last UID:    %08X (dec: %u)", d->last_uid, d->last_uid);
	shell_print(shell, "  Total reads: %u", d->total_reads);
	shell_print(shell, "  Errors:      %u", d->error_count);
	shell_print(shell, "  Valid:       %s", d->valid ? "yes" : "no");
}

/* Driver registration */
const struct app_device_driver promag_mf7s_driver = {
	.name = "promag_mf7s",
	.type = APP_DEVICE_TYPE_PROMAG_MF7S,
	.init = init,
	.sample = sample,
	.deinit = NULL,
	.calibrate = NULL,
	.reset = NULL,
	.config = NULL,
	.print_data = print_data,
};

/*
 * Shell commands
 */

static int find_promag_device(void)
{
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type == APP_DEVICE_TYPE_PROMAG_MF7S) {
			return i;
		}
	}
	return -1;
}

static int cmd_promag_read_info(const struct shell *shell, size_t argc, char **argv)
{
	int device_idx = find_promag_device();
	if (device_idx < 0) {
		shell_error(shell, "Promag MF7S not configured");
		return -ENODEV;
	}

	shell_print(shell, "Reading device info...");

	listener_pause();

	int ret = send_mf7s_command("V");
	if (ret) {
		shell_error(shell, "Failed to send command: %d", ret);
		listener_resume();
		return ret;
	}

	char response[32];
	ret = receive_mf7s_response(response, sizeof(response), CMD_RESPONSE_MS);
	if (ret) {
		shell_error(shell, "No response: %d", ret);
		listener_resume();
		return ret;
	}

	shell_print(shell, "Firmware: %s", response);

	k_sleep(K_MSEC(100));

	/* Read device name via 'D' command */
	ret = send_mf7s_command("D");
	if (ret) {
		shell_error(shell, "Failed to send D command: %d", ret);
		listener_resume();
		return ret;
	}

	ret = receive_mf7s_response(response, sizeof(response), CMD_RESPONSE_MS);
	if (ret) {
		shell_error(shell, "No device name response: %d", ret);
		listener_resume();
		return ret;
	}

	shell_print(shell, "Device:   %s", response);

	listener_resume();
	return 0;
}

static int cmd_promag_notify(const struct shell *shell, size_t argc, char **argv)
{
	int device_idx = find_promag_device();
	if (device_idx < 0) {
		shell_error(shell, "Promag MF7S not configured");
		return -ENODEV;
	}

	listener_pause();

	/* JH: Treble beep continue */
	int ret = send_mf7s_command("JH");
	if (ret) {
		shell_error(shell, "Failed to send JH: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(500));

	/* JM: Mediant beep continue */
	ret = send_mf7s_command("JM");
	if (ret) {
		shell_error(shell, "Failed to send JM: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(500));

	/* JL: Bass beep continue */
	ret = send_mf7s_command("JL");
	if (ret) {
		shell_error(shell, "Failed to send JL: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(500));

	/* JO: Beep continue off */
	ret = send_mf7s_command("JO");
	if (ret) {
		shell_error(shell, "Failed to send JO: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(500));

	/* J1: Green LED on */
	ret = send_mf7s_command("J1");
	if (ret) {
		shell_error(shell, "Failed to send J1: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(1000));

	/* J2: Green LED off */
	ret = send_mf7s_command("J2");
	if (ret) {
		shell_error(shell, "Failed to send J2: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(500));

	/* J3: Red LED on */
	ret = send_mf7s_command("J3");
	if (ret) {
		shell_error(shell, "Failed to send J3: %d", ret);
		listener_resume();
		return ret;
	}

	k_sleep(K_MSEC(1000));

	/* J4: Red LED off */
	ret = send_mf7s_command("J4");
	if (ret) {
		shell_error(shell, "Failed to send J4: %d", ret);
		listener_resume();
		return ret;
	}

	shell_print(shell, "Notification sent");

	listener_resume();
	return 0;
}

static int cmd_promag_sampling(const struct shell *shell, size_t argc, char **argv)
{
	int device_idx = find_promag_device();
	if (device_idx < 0) {
		shell_error(shell, "Promag MF7S not configured");
		return -ENODEV;
	}

	if (atomic_get(&m_sampling_mode)) {
		shell_error(shell, "Sampling already active");
		return -EBUSY;
	}

	/* Parse optional timeout */
	int timeout_s = 10;
	if (argc >= 2) {
		timeout_s = atoi(argv[1]);
		if (timeout_s <= 0) {
			timeout_s = 10;
		}
	}

	shell_print(shell, "Sampling mode active (idle timeout: %ds)", timeout_s);
	shell_print(shell, "Present cards to the reader...");

	/* Enable sampling mode */
	m_sampling_shell = shell;
	m_sampling_timeout_s = timeout_s;
	m_sampling_idle_deadline = k_uptime_get() + (timeout_s * 1000);
	atomic_set(&m_sampling_mode, 1);

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.sampling_active = true;
	k_mutex_unlock(&m_data_mutex);

	/* Wait for idle timeout */
	while (atomic_get(&m_sampling_mode)) {
		int64_t now = k_uptime_get();
		if (now >= m_sampling_idle_deadline) {
			break;
		}
		k_sleep(K_MSEC(200));
	}

	/* Disable sampling mode */
	atomic_set(&m_sampling_mode, 0);
	m_sampling_shell = NULL;

	k_mutex_lock(&m_data_mutex, K_FOREVER);
	m_data.sampling_active = false;
	k_mutex_unlock(&m_data_mutex);

	shell_print(shell, "Sampling finished (total reads: %u)", m_data.total_reads);

	return 0;
}

/* clang-format off */

const struct shell_static_entry promag_mf7s_shell_subcmds[] = {
	SHELL_CMD_ARG(read_info, NULL, "Read device info", cmd_promag_read_info, 1, 0),
	SHELL_CMD_ARG(notify, NULL, "Beep notification + LED", cmd_promag_notify, 1, 0),
	SHELL_CMD_ARG(sampling, NULL, "Sampling mode: sampling [timeout_s]", cmd_promag_sampling, 1, 1),
	SHELL_SUBCMD_SET_END
};

const union shell_cmd_entry promag_mf7s_shell_cmds = {
	.entry = promag_mf7s_shell_subcmds
};

/* clang-format on */
