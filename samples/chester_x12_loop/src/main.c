/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

RING_BUF_DECLARE(m_rx_ring_buf, 256);
RING_BUF_DECLARE(m_tx_ring_buf, 256);

static void rx_work_handler(struct k_work *work)
{
	for (;;) {
		uint8_t buf[16];
		uint32_t len = ring_buf_get(&m_rx_ring_buf, buf, sizeof(buf));

		if (len) {
			LOG_HEXDUMP_INF(buf, len, "Buffer read");
		}

		if (len < sizeof(buf)) {
			break;
		}
	}
}

static K_WORK_DEFINE(m_rx_work, rx_work_handler);

void uart_callback(const struct device *dev, void *user_data)
{
	int ret;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (uart_irq_rx_ready(dev)) {
		LOG_DBG("RX interrupt");

		for (;;) {
			uint8_t buf[16];
			int bytes_read = uart_fifo_read(dev, buf, sizeof(buf));
			if (bytes_read < 0) {
				LOG_ERR("Call `uart_fifo_read` failed: %d", bytes_read);
				k_oops();
			}

			if (!bytes_read) {
				break;
			}

			uint32_t written = ring_buf_put(&m_rx_ring_buf, buf, bytes_read);

			LOG_DBG("Wrote %u byte(s) to RX ring buffer", written);

			k_work_submit(&m_rx_work);

			if (written != bytes_read) {
				LOG_ERR("Not enough space in RX ring buffer");
				break;
			}
		}
	}

	if (uart_irq_tx_ready(dev)) {
		LOG_DBG("TX interrupt");

		for (;;) {
			uint8_t *buf;
			uint32_t claimed = ring_buf_get_claim(&m_tx_ring_buf, &buf, 16);
			if (!claimed) {
				uart_irq_tx_disable(dev);
				break;
			}

			LOG_DBG("Claimed %u byte(s) from TX ring buffer", claimed);

			int bytes_sent = uart_fifo_fill(dev, buf, claimed);
			if (bytes_sent < 0) {
				LOG_ERR("Call `uart_fifo_fill` failed: %d", bytes_sent);
				k_oops();
			}

			ret = ring_buf_get_finish(&m_tx_ring_buf, bytes_sent);
			if (ret) {
				LOG_ERR("Call `ring_buf_get_finish` failed: %d", ret);
				k_oops();
			}

			LOG_DBG("Consumed %u byte(s) from TX ring buffer", bytes_sent);

			if (bytes_sent != claimed) {
				break;
			}
		}
	}
}

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x12_sc16is740_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	uart_irq_callback_user_data_set(dev, uart_callback, NULL);
	uart_irq_rx_enable(dev);

	for (;;) {
		static int counter;

		char buf[64];
		ret = snprintf(buf, sizeof(buf), "Hello CHESTER (round %d)", ++counter);
		if (ret < 0) {
			LOG_ERR("Call `snprintf` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Writing message: %s", buf);

		uint32_t written = ring_buf_put(&m_tx_ring_buf, buf, strlen(buf));

		LOG_DBG("Wrote %u byte(s) to TX ring buffer", written);

		if (written != strlen(buf)) {
			LOG_ERR("Not enough space in TX ring buffer");
		}

		uart_irq_tx_enable(dev);

		k_sleep(K_MSEC(50));
	}

	return 0;
}
