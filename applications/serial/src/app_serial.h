/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SERIAL_H_
#define APP_SERIAL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>

/**
 * @brief Compile-time check: is any serial UART bridge shield (CTR-X2 RS-485
 *        or CTR-X12 RS-232) present in the device tree?
 *
 * Use to skip serial initialization entirely on builds without a shield.
 */
#define APP_SERIAL_HW_PRESENT                                                                      \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x2_sc16is740_a), okay) ||                             \
	 DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x12_sc16is740_a), okay))

/* Forward declarations */
struct shell;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize serial UART interface
 *
 * Configures UART with parameters from app_config and sets up
 * interrupt-driven or async mode for data reception.
 *
 * @return 0 on success, negative error code on failure
 */
int app_serial_init(void);

/**
 * @brief Send data over serial interface
 *
 * @param data Pointer to data buffer to send
 * @param len Length of data in bytes
 * @return Number of bytes sent, or negative error code
 */
int app_serial_send(const uint8_t *data, size_t len);

/**
 * @brief Receive data from serial interface with timeout
 *
 * Blocks until data is received or timeout expires.
 *
 * @param buf Pointer to buffer for received data
 * @param max_len Maximum number of bytes to receive
 * @param out_len Pointer to store actual number of bytes received
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, -ETIMEDOUT if no data received
 */
int app_serial_receive_timeout(uint8_t *buf, size_t max_len, size_t *out_len, uint32_t timeout_ms);

/**
 * @brief Flush RX buffer
 *
 * Discards all pending received data.
 */
void app_serial_flush_rx(void);

/**
 * @brief Block until queued transmit data has been clocked out
 *
 * app_serial_send() returns as soon as the data is queued, not when it has been
 * transmitted, so anything that tears down the link straight afterwards can cut the
 * frame short. This waits for the TX ring buffer to empty and then for the bridge's
 * own TX FIFO to drain.
 *
 * The FIFO wait is a computed worst case rather than a real check: the SC16IS7xx driver
 * implements no irq_tx_complete, so the transmitter-empty (LSR TEMT) bit is not reachable
 * through the UART API. If that is added later this should poll it instead.
 *
 * @param timeout_ms Maximum time to wait for the ring buffer to empty
 * @return 0 on success, -ETIMEDOUT if data was still queued at the deadline
 */
int app_serial_tx_drain(uint32_t timeout_ms);

/**
 * @brief Suspend or resume the UART bridge via device power management
 *
 * The bridge is left suspended while idle (a resumed SC16IS7xx draws ~5.4 mA
 * continuously). app_serial_send() resumes it on demand, so this only needs calling
 * explicitly by a driver that wants to drop the idle current after its own
 * transaction, or to hold the bridge awake for unsolicited receive.
 *
 * Suspending calls app_serial_tx_drain() first, so it is safe to call directly after
 * app_serial_send() without truncating the frame.
 *
 * Idempotent: requesting the state the device is already in succeeds.
 *
 * @param resume true to resume, false to suspend
 * @return 0 on success, negative errno otherwise
 */
int app_serial_pm(bool resume);

/**
 * @brief Force the TX line low (break) or release it to idle high
 *
 * Peripherals that expect their RX input parked at logic 0 while idle need this;
 * a UART TX pin otherwise idles high forever.
 *
 * @param set true to hold TX low, false to return to normal idle-high
 * @return 0 on success, negative errno otherwise
 */
int app_serial_set_break(bool set);

/**
 * @brief Re-apply the UART settings, as app_serial_init() does at boot
 *
 * @return 0 on success, negative errno otherwise
 */
int app_serial_reconfigure(void);

/**
 * @brief Run the low -> high wake sequence on the TX line
 *
 * Holds TX low for @p low_ms so the peer enters power-save, then releases it high for
 * @p high_ms. The Piketronic RPP-U requires this 0 -> 1 transition held for 2 s before
 * it will accept a command; without it, the probe answers exactly once per UART
 * initialization and stays silent afterwards.
 *
 * @param low_ms  Time to hold TX low
 * @param high_ms Time to hold TX high before returning
 * @return 0 on success, negative errno otherwise
 */
int app_serial_wake(uint32_t low_ms, uint32_t high_ms);

/**
 * @brief Shell command to test serial transmission
 *
 * Usage: serial send <data>
 */
int cmd_serial_send(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to test serial reception
 *
 * Usage: serial receive
 */
int cmd_serial_receive(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to suspend/resume the UART bridge
 *
 * Usage: serial pm <0|1>
 */
int cmd_serial_pm(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to assert/deassert the TX break
 *
 * Usage: serial break <0|1>
 */
int cmd_serial_break(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to re-apply the UART configuration
 *
 * Usage: serial reinit
 */
int cmd_serial_reinit(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to run the wake sequence and then send a frame
 *
 * Usage: serial wake <hex> [timeout_s] [high_ms]
 */
int cmd_serial_wake_send(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_H_ */
