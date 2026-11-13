/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_serial.h"
#include "app_config.h"

/* CHESTER includes */
#include <chester/drv_sc16is7xx.h>

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

/* TX FIFO depth of the SC16IS7xx bridge, in bytes */
#define TX_FIFO_DEPTH 64

/* Upper bound on how long app_serial_pm(false) waits for queued TX to be handed to the
 * bridge before suspending anyway. Generous: a full 512-byte ring buffer at 1200 baud is
 * around 5 s, and suspending early only truncates a frame that was already stuck.
 */
#define TX_DRAIN_TIMEOUT_MS 6000

/* Used when the configured baud rate is unusable (0), so the drain still waits */
#define TX_DRAIN_FALLBACK_MS 100
RING_BUF_DECLARE(m_rx_ring_buf, RING_BUF_SIZE);
RING_BUF_DECLARE(m_tx_ring_buf, RING_BUF_SIZE);

#if HAS_SERIAL_DEVICE
static const struct device *m_uart_dev = NULL;
/* Set when app_serial_send() queues data. A drain takes it before it waits and hands it
 * back if it times out, so a send that lands during the wait keeps its flag. Lets a drain
 * skip both waits when nothing has been queued since the last drain started - most
 * importantly the suspend at the end of app_serial_init(), where nothing was ever sent.
 */
static atomic_t m_tx_dirty = ATOMIC_INIT(0);
/* One drain at a time: a second drain starting while the first still waits would find the
 * flag already taken, skip the wait and let its caller suspend the bridge under the first
 * frame.
 */
static K_MUTEX_DEFINE(m_drain_mutex);
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
	LOG_ERR("No serial device (CTR-X2/X12) available in device tree");
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

	/* Get UART device (use local until init succeeds — keeps m_uart_dev
	 * NULL on failure so app_serial_send() short-circuits safely)
	 */
	const struct device *uart_dev = DEVICE_DT_GET(SERIAL_DEVICE_NODE);
	if (!device_is_ready(uart_dev)) {
		LOG_ERR("UART device not ready (SC16IS740 chip absent or wrong shield?)");
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

	ret = uart_configure(uart_dev, &uart_cfg);
	if (ret) {
		LOG_ERR("uart_configure failed: %d", ret);
		return ret;
	}

	/* Set up interrupt-driven UART */
	ret = uart_irq_callback_user_data_set(uart_dev, uart_callback, NULL);
	if (ret) {
		LOG_ERR("uart_irq_callback_user_data_set failed: %d", ret);
		return ret;
	}

	uart_irq_rx_enable(uart_dev);

	/* Publish only after full init success */
	m_uart_dev = uart_dev;

	LOG_INF("Serial interface: %s, %d baud, %d-%c-%d",
		SERIAL_INTERFACE_NAME,
		g_app_config.serial_baudrate,
		g_app_config.serial_data_bits,
		g_app_config.serial_parity == 0 ? 'N' : (g_app_config.serial_parity == 1 ? 'O' : 'E'),
		g_app_config.serial_stop_bits);

	/* Leave the SC16IS7xx bridge suspended while idle.
	 *
	 * A resumed bridge draws ~5.4 mA continuously. Until now it was configured once here
	 * and then left powered for the lifetime of the device, so every deployment paid that
	 * current whether or not any data was moving.
	 *
	 * app_serial_send() resumes on demand, so device drivers keep working untouched. A
	 * driver that wants the idle current back down should suspend after its transaction.
	 *
	 * Trade-off: a suspended bridge cannot hear unsolicited traffic. `transparent` mode is
	 * the raw passthrough (`serial send` / `recv` and the driverless legacy types), which
	 * must catch bytes it never asked for, so it keeps the bridge awake as it always did.
	 * Polled drivers still suspend after their own transactions there.
	 */
	if (g_app_config.serial_mode == APP_CONFIG_SERIAL_MODE_TRANSPARENT) {
		LOG_INF("Transparent mode: UART bridge kept awake for unsolicited RX");
	} else {
		ret = app_serial_pm(false);
		if (ret) {
			LOG_WRN("Could not suspend UART bridge: %d (idle current will be ~5.4 mA)",
				ret);
		}
	}

	return 0;
#endif
}

#if HAS_SERIAL_DEVICE
/* Worst-case time for the bridge's TX FIFO and shift register to clock out, at the
 * configured line settings. Used because transmitter-empty is not observable - see
 * app_serial_tx_drain().
 */
static uint32_t tx_fifo_drain_ms(void)
{
	uint32_t baud = g_app_config.serial_baudrate;

	if (baud == 0) {
		return TX_DRAIN_FALLBACK_MS;
	}

	/* start + data + optional parity + stop bits */
	uint32_t bits = 1 + g_app_config.serial_data_bits + g_app_config.serial_stop_bits +
			(g_app_config.serial_parity != APP_CONFIG_SERIAL_PARITY_NONE ? 1 : 0);

	/* +1 for the byte already in the shift register */
	uint32_t ms = ((TX_FIFO_DEPTH + 1) * bits * 1000U + baud - 1) / baud;

	return ms ? ms : 1;
}
#endif

int app_serial_tx_drain(uint32_t timeout_ms)
{
#if !HAS_SERIAL_DEVICE
	return -ENOTSUP;
#else
	if (m_uart_dev == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&m_drain_mutex, K_FOREVER);

	/* Take the flag before waiting: an app_serial_send() from another thread while this
	 * drain waits sets it again, so the next drain waits for that data too. This only keeps
	 * the flag right - the suspend that follows this drain can still cut such a frame, so
	 * concurrent transactions on the bridge stay unsupported. Nothing queued since the last
	 * drain started (e.g. the suspend at the end of app_serial_init(), before anything was
	 * ever sent) means there is nothing to wait for, and the worst-case FIFO sleep below
	 * would just be dead time.
	 */
	if (!atomic_cas(&m_tx_dirty, 1, 0)) {
		k_mutex_unlock(&m_drain_mutex);
		return 0;
	}

	int64_t deadline = k_uptime_get() + timeout_ms;

	/* Wait for the ISR to hand every queued byte to the bridge. */
	while (!ring_buf_is_empty(&m_tx_ring_buf)) {
		if (k_uptime_get() > deadline) {
			LOG_WRN("TX drain timed out, %u bytes still queued",
				ring_buf_size_get(&m_tx_ring_buf));
			/* Not drained: hand the flag back so the next drain waits again. */
			atomic_set(&m_tx_dirty, 1);
			k_mutex_unlock(&m_drain_mutex);
			return -ETIMEDOUT;
		}
		k_sleep(K_MSEC(1));
	}

	/* The bytes now sit in the bridge's own TX FIFO, which cannot be polled: the
	 * SC16IS7xx driver implements no irq_tx_complete, so the transmitter-empty (LSR
	 * TEMT) bit is not reachable through the UART API. Wait the worst-case time for a
	 * full FIFO plus the shift register to clock out instead.
	 */
	k_sleep(K_MSEC(tx_fifo_drain_ms()));

	k_mutex_unlock(&m_drain_mutex);

	return 0;
#endif
}

int app_serial_pm(bool resume)
{
#if !HAS_SERIAL_DEVICE
	return -ENOTSUP;
#else
	if (m_uart_dev == NULL) {
		return -EINVAL;
	}

	/* Suspending mid-transmission truncates the frame: app_serial_send() returns as soon
	 * as the data is queued, not when it has been clocked out. Drain first so callers
	 * cannot cut their own frame short by suspending straight after sending.
	 */
	if (!resume) {
		(void)app_serial_tx_drain(TX_DRAIN_TIMEOUT_MS);
	}

	int ret = pm_device_action_run(m_uart_dev, resume ? PM_DEVICE_ACTION_RESUME
							 : PM_DEVICE_ACTION_SUSPEND);
	/* Already in the requested state - nothing to do, not an error. */
	if (ret == -EALREADY) {
		return 0;
	}

	return ret;
#endif
}

int app_serial_reconfigure(void)
{
#if !HAS_SERIAL_DEVICE
	return -ENOTSUP;
#else
	if (m_uart_dev == NULL) {
		return -EINVAL;
	}

	/* Re-apply the UART settings, mimicking what app_serial_init() does at boot. The
	 * Piketronic RPP-U only accepts commands shortly after that happens, so this re-arms
	 * it without rebooting.
	 */
	struct uart_config uart_cfg = {
		.baudrate = g_app_config.serial_baudrate,
		.data_bits = UART_CFG_DATA_BITS_8,
		.stop_bits = (g_app_config.serial_stop_bits == 1) ? UART_CFG_STOP_BITS_1
								  : UART_CFG_STOP_BITS_2,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	switch (g_app_config.serial_parity) {
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

	switch (g_app_config.serial_data_bits) {
	case 7:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_7;
		break;
	case 9:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_9;
		break;
	default:
		uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
		break;
	}

	/* uart_configure() writes the bridge's divisor/line registers over I2C, so it needs the
	 * chip powered. The bridge is left suspended while idle (see app_serial_init).
	 */
	int pm_ret = app_serial_pm(true);
	if (pm_ret) {
		LOG_ERR("Could not resume UART bridge for reconfigure: %d", pm_ret);
		return pm_ret;
	}

	return uart_configure(m_uart_dev, &uart_cfg);
#endif
}

int app_serial_set_break(bool set)
{
#if !HAS_SERIAL_DEVICE
	return -ENOTSUP;
#else
	if (m_uart_dev == NULL) {
		return -EINVAL;
	}

	/* Touching the break bit is a register write over I2C, so the bridge has to be powered.
	 * It is left suspended while idle (see app_serial_init), so resume first. Callers that
	 * park the line and then power down still end up suspended, because they call
	 * app_serial_pm(false) after this.
	 */
	int pm_ret = app_serial_pm(true);
	if (pm_ret) {
		LOG_ERR("Could not resume UART bridge for break: %d", pm_ret);
		return pm_ret;
	}

	return drv_sc16is7xx_set_break(m_uart_dev, set);
#endif
}

int app_serial_wake(uint32_t low_ms, uint32_t high_ms)
{
	int ret;

	/* Release the line if it was parked low, then re-initialize the bridge.
	 *
	 * Measured on a Piketronic RPP-U (S/N 24123): a break low->high edge alone does NOT
	 * re-arm it, but re-running uart_configure() (or a PM suspend/resume cycle) does - the
	 * probe otherwise answers exactly once per UART init and then stays silent. low_ms is
	 * how long the line stays parked low beforehand, high_ms the settle time before the
	 * command (the vendor spec asks for 2 s).
	 */
	ret = app_serial_set_break(true);
	if (ret) {
		return ret;
	}

	k_sleep(K_MSEC(low_ms));

	ret = app_serial_set_break(false);
	if (ret) {
		return ret;
	}

	ret = app_serial_reconfigure();
	if (ret) {
		LOG_ERR("app_serial_reconfigure failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(high_ms));

	/* Drop the framing garbage the break edge may have produced */
	app_serial_flush_rx();

	return 0;
}

int app_serial_send(const uint8_t *data, size_t len)
{
#if !HAS_SERIAL_DEVICE
	return -ENOTSUP;
#else
	if (m_uart_dev == NULL || data == NULL || len == 0) {
		return -EINVAL;
	}

	/* The bridge is left suspended while idle (see app_serial_init), so resume before
	 * touching it - a suspended SC16IS7xx cannot clock bytes out. Idempotent, so callers
	 * that manage power themselves are unaffected.
	 */
	int pm_ret = app_serial_pm(true);
	if (pm_ret) {
		LOG_ERR("Could not resume UART bridge for TX: %d", pm_ret);
		return pm_ret;
	}

	k_mutex_lock(&m_uart_mutex, K_FOREVER);

	uint32_t written = ring_buf_put(&m_tx_ring_buf, data, len);

	if (written != len) {
		LOG_WRN("TX ring buffer full, only wrote %u of %zu bytes", written, len);
	}

	/* Mark that a drain now has real work to wait for (see app_serial_tx_drain). */
	atomic_set(&m_tx_dirty, 1);

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

int cmd_serial_pm(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: %s <0|1>   (0 = suspend, 1 = resume)", argv[0]);
		return -EINVAL;
	}

	bool resume = atoi(argv[1]) != 0;

	int ret = app_serial_pm(resume);
	if (ret) {
		shell_error(shell, "PM %s failed: %d", resume ? "resume" : "suspend", ret);
		return ret;
	}

	shell_print(shell, "UART bridge %s", resume ? "resumed" : "suspended");

	return 0;
}

int cmd_serial_break(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: %s <0|1>", argv[0]);
		return -EINVAL;
	}

	bool set = atoi(argv[1]) != 0;

	int ret = app_serial_set_break(set);
	if (ret) {
		shell_error(shell, "Failed to %s break: %d", set ? "assert" : "deassert", ret);
		return ret;
	}

	shell_print(shell, "Break %s - TX held %s", set ? "asserted" : "deasserted",
		    set ? "LOW" : "HIGH (idle)");

	return 0;
}

int cmd_serial_reinit(const struct shell *shell, size_t argc, char **argv)
{
	int ret = app_serial_reconfigure();
	if (ret) {
		shell_error(shell, "Reconfigure failed: %d", ret);
		return ret;
	}

	shell_print(shell, "UART reconfigured (%d baud)", g_app_config.serial_baudrate);

	return 0;
}

int cmd_serial_wake_send(const struct shell *shell, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(shell, "Usage: %s <hex_string> [timeout_s] [high_ms]", argv[0]);
		shell_print(shell, "Example: %s 4000014445 6", argv[0]);
		return -EINVAL;
	}

	uint8_t tx_buf[256];
	int timeout_s = 6;
	uint32_t high_ms = 2500; /* spec: hold RX_in at 1 for 2 s before the command */

	if (argc >= 3) {
		timeout_s = atoi(argv[2]);
		if (timeout_s <= 0) {
			timeout_s = 6;
		}
	}

	if (argc >= 4) {
		high_ms = (uint32_t)atoi(argv[3]);
	}

	int tx_len = hex_string_to_bytes(argv[1], tx_buf, sizeof(tx_buf));
	if (tx_len < 0) {
		shell_error(shell, "Invalid hex string (must be even length, 0-9 a-f A-F)");
		return -EINVAL;
	}

	shell_print(shell, "Wake: park low 250 ms, reinit, settle %u ms...", high_ms);

	int ret = app_serial_wake(250, high_ms);
	if (ret) {
		shell_error(shell, "Wake sequence failed: %d", ret);
		return ret;
	}

	shell_print(shell, "TX (%d bytes):", tx_len);
	shell_hexdump(shell, tx_buf, tx_len);

	ret = app_serial_send(tx_buf, tx_len);
	if (ret < 0) {
		shell_error(shell, "Failed to send: %d", ret);
		return ret;
	}

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
