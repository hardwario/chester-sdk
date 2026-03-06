/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SERIAL_H_
#define APP_SERIAL_H_

#include <stddef.h>
#include <stdint.h>

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

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_H_ */
