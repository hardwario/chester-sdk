/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_serial.h"
#include "app_config.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/ring_buffer.h>

/* Standard includes */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(app_serial, LOG_LEVEL_DBG);

/* Check which serial device is available */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x2_sc16is740_a), okay)
#define SERIAL_DEVICE_NODE DT_NODELABEL(ctr_x2_sc16is740_a)
#define HAS_SERIAL_DEVICE 1
#define SERIAL_INTERFACE_NAME "RS-485 (X2)"
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x12_sc16is740_a), okay)
#define SERIAL_DEVICE_NODE DT_NODELABEL(ctr_x12_sc16is740_a)
#define HAS_SERIAL_DEVICE 1
#define SERIAL_INTERFACE_NAME "RS-232 (X12)"
#else
#define HAS_SERIAL_DEVICE 0
#define SERIAL_INTERFACE_NAME "None"
#endif

/* Ring buffers for RX and TX */
#define RING_BUF_SIZE 512
RING_BUF_DECLARE(m_rx_ring_buf, RING_BUF_SIZE);
RING_BUF_DECLARE(m_tx_ring_buf, RING_BUF_SIZE);

#if HAS_SERIAL_DEVICE
static const struct device *m_uart_dev = NULL;
#endif
static struct k_mutex m_uart_mutex;

#if HAS_SERIAL_DEVICE
/* UART interrupt callback */
static void uart_callback(const struct device *dev, void *user_data)
{
	int ret;

	if (!uart_irq_update(dev)) {
		return;
	}

	/* Handle RX interrupt */
	if (uart_irq_rx_ready(dev)) {
		for (;;) {
			uint8_t buf[32];
			int bytes_read = uart_fifo_read(dev, buf, sizeof(buf));

			if (bytes_read < 0) {
				LOG_ERR("uart_fifo_read failed: %d", bytes_read);
				break;
			}

			if (!bytes_read) {
				break;
			}

			uint32_t written = ring_buf_put(&m_rx_ring_buf, buf, bytes_read);

			if (written != bytes_read) {
				LOG_WRN("RX ring buffer full, dropped %u bytes",
					bytes_read - written);
				break;
			}
		}
	}

	/* Handle TX interrupt */
	if (uart_irq_tx_ready(dev)) {
		for (;;) {
			uint8_t *buf;
			uint32_t claimed = ring_buf_get_claim(&m_tx_ring_buf, &buf, 32);

			if (!claimed) {
				uart_irq_tx_disable(dev);
				break;
			}

			int bytes_sent = uart_fifo_fill(dev, buf, claimed);
			if (bytes_sent < 0) {
				LOG_ERR("uart_fifo_fill failed: %d", bytes_sent);
				break;
			}

			ret = ring_buf_get_finish(&m_tx_ring_buf, bytes_sent);
			if (ret) {
				LOG_ERR("ring_buf_get_finish failed: %d", ret);
				break;
			}

			if (bytes_sent != claimed) {
				break;
			}
		}
	}
}
#endif /* HAS_SERIAL_DEVICE */

int app_serial_init(void)
{
#if !HAS_SERIAL_DEVICE
	LOG_WRN("No serial device (CTR-X2/X12) available in device tree");
	return -ENOTSUP;
#else
	int ret;

	LOG_INF("Initializing serial interface");

	/* Initialize mutex */
	ret = k_mutex_init(&m_uart_mutex);
	if (ret) {
		LOG_ERR("k_mutex_init failed: %d", ret);
		return ret;
	}

	/* Get UART device */
	m_uart_dev = DEVICE_DT_GET(SERIAL_DEVICE_NODE);
	if (!device_is_ready(m_uart_dev)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}

	/* Configure UART parameters from app_config */
	struct uart_config uart_cfg = {
		.baudrate = g_app_config.serial_baudrate,
		.data_bits = UART_CFG_DATA_BITS_8, /* Default to 8 */
		.stop_bits = (g_app_config.serial_stop_bits == 1) ? UART_CFG_STOP_BITS_1
								  : UART_CFG_STOP_BITS_2,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	/* Map parity from config */
	switch (g_app_config.serial_parity) {
	case APP_CONFIG_SERIAL_PARITY_NONE:
		uart_cfg.parity = UART_CFG_PARITY_NONE;
		break;
	case APP_CONFIG_SERIAL_PARITY_ODD:
		uart_cfg.parity = UART_CFG_PARITY_ODD;
		break;
	case APP_CONFIG_SERIAL_PARITY_EVEN:
		uart_cfg.parity = UART_CFG_PARITY_EVEN;
		break;
	default:
		uart_cfg.parity = UART_CFG_PARITY_NONE;
		break;
	}

	/* Map data bits from config */
	switch (g_app_config.serial_data_bits) {
	case 7:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_7;
		break;
	case 8:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
		break;
	case 9:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_9;
		break;
	default:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
		break;
	}

	ret = uart_configure(m_uart_dev, &uart_cfg);
	if (ret) {
		LOG_ERR("uart_configure failed: %d", ret);
		return ret;
	}

	/* Set up interrupt-driven UART */
	ret = uart_irq_callback_user_data_set(m_uart_dev, uart_callback, NULL);
	if (ret) {
		LOG_ERR("uart_irq_callback_user_data_set failed: %d", ret);
		return ret;
	}

	uart_irq_rx_enable(m_uart_dev);

	LOG_INF("Serial interface: %s, %d baud, %d-%c-%d",
		SERIAL_INTERFACE_NAME,
		g_app_config.serial_baudrate,
		g_app_config.serial_data_bits,
		g_app_config.serial_parity == 0 ? 'N' : (g_app_config.serial_parity == 1 ? 'O' : 'E'),
		g_app_config.serial_stop_bits);

	return 0;
#endif
}

int app_serial_send(const uint8_t *data, size_t len)
{
#if !HAS_SERIAL_DEVICE
	return -ENOTSUP;
#else
	if (m_uart_dev == NULL || data == NULL || len == 0) {
		return -EINVAL;
	}

	k_mutex_lock(&m_uart_mutex, K_FOREVER);

	uint32_t written = ring_buf_put(&m_tx_ring_buf, data, len);

	if (written != len) {
		LOG_WRN("TX ring buffer full, only wrote %u of %zu bytes", written, len);
	}

	uart_irq_tx_enable(m_uart_dev);

	k_mutex_unlock(&m_uart_mutex);

	return written;
#endif
}

int app_serial_receive_timeout(uint8_t *buf, size_t max_len, size_t *out_len, uint32_t timeout_ms)
{
	if (buf == NULL || out_len == NULL || max_len == 0) {
		return -EINVAL;
	}

	int64_t start = k_uptime_get();
	int64_t last_rx_time = 0;
	size_t total = 0;

	/* Inter-character timeout: wait 100ms after last received byte */
	const uint32_t inter_char_timeout_ms = 100;

	while ((k_uptime_get() - start) < timeout_ms && total < max_len) {
		k_mutex_lock(&m_uart_mutex, K_FOREVER);
		uint32_t avail = ring_buf_size_get(&m_rx_ring_buf);
		if (avail > 0) {
			uint32_t to_read = MIN(avail, max_len - total);
			uint32_t read = ring_buf_get(&m_rx_ring_buf, buf + total, to_read);
			total += read;
			last_rx_time = k_uptime_get();
		}
		k_mutex_unlock(&m_uart_mutex);

		/* Check inter-character timeout */
		if (total > 0 && avail == 0) {
			/* Got some data, no more available - wait for inter-char timeout */
			if ((k_uptime_get() - last_rx_time) >= inter_char_timeout_ms) {
				break;
			}
		}

		k_sleep(K_MSEC(1));
	}

	*out_len = total;
	return (total > 0) ? 0 : -ETIMEDOUT;
}

void app_serial_flush_rx(void)
{
	k_mutex_lock(&m_uart_mutex, K_FOREVER);
	ring_buf_reset(&m_rx_ring_buf);
	k_mutex_unlock(&m_uart_mutex);
}

/* Shell commands */

static int hex_char_to_nibble(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

static int hex_string_to_bytes(const char *hex, uint8_t *out, size_t out_size)
{
	size_t hex_len = strlen(hex);

	if (hex_len % 2 != 0) {
		return -EINVAL;
	}

	size_t byte_len = hex_len / 2;
	if (byte_len > out_size) {
		return -ENOSPC;
	}

	for (size_t i = 0; i < byte_len; i++) {
		int high = hex_char_to_nibble(hex[i * 2]);
		int low = hex_char_to_nibble(hex[i * 2 + 1]);

		if (high < 0 || low < 0) {
			return -EINVAL;
		}

		out[i] = (high << 4) | low;
	}

	return byte_len;
}

int cmd_serial_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: %s <hex_string> [timeout_s]", argv[0]);
		shell_print(shell, "Example: %s 0102030405 2", argv[0]);
		return -EINVAL;
	}

	uint8_t tx_buf[256];
	int timeout_s = 1;

	if (argc >= 3) {
		timeout_s = atoi(argv[2]);
		if (timeout_s <= 0) {
			timeout_s = 1;
		}
	}

	int tx_len = hex_string_to_bytes(argv[1], tx_buf, sizeof(tx_buf));
	if (tx_len < 0) {
		shell_error(shell, "Invalid hex string (must be even length, 0-9 a-f A-F)");
		return -EINVAL;
	}

	/* Flush RX before sending */
	app_serial_flush_rx();

	shell_print(shell, "TX (%d bytes):", tx_len);
	shell_hexdump(shell, tx_buf, tx_len);

	int ret = app_serial_send(tx_buf, tx_len);
	if (ret < 0) {
		shell_error(shell, "Failed to send: %d", ret);
		return ret;
	}

	shell_print(shell, "Waiting %d s for response...", timeout_s);

	/* Read response with timeout */
	uint8_t rx_buf[256];
	size_t rx_len = 0;
	app_serial_receive_timeout(rx_buf, sizeof(rx_buf), &rx_len, timeout_s * 1000);

	if (rx_len > 0) {
		shell_print(shell, "RX (%zu bytes):", rx_len);
		shell_hexdump(shell, rx_buf, rx_len);
	} else {
		shell_warn(shell, "No response received");
	}

	return 0;
}

int cmd_serial_receive(const struct shell *shell, size_t argc, char **argv)
{
	int timeout_s = 1;

	if (argc >= 2) {
		timeout_s = atoi(argv[1]);
		if (timeout_s <= 0) {
			timeout_s = 1;
		}
	}

	shell_print(shell, "Receiving for %d s...", timeout_s);

	uint8_t buf[256];
	size_t len = 0;
	app_serial_receive_timeout(buf, sizeof(buf), &len, timeout_s * 1000);

	if (len > 0) {
		shell_print(shell, "Received %zu bytes:", len);
		shell_hexdump(shell, buf, len);
	} else {
		shell_print(shell, "No data received");
	}

	return 0;
}
