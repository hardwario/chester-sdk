/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MODBUS_H_
#define APP_MODBUS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct shell;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Modbus RTU client
 *
 * Initializes Modbus RTU client with parameters from app_config.
 * Configures baud rate, parity, stop bits, and timeout.
 *
 * @return 0 on success, negative error code on failure
 */
int app_modbus_init(void);

/**
 * @brief Enable Modbus interface (power up SC16IS740)
 *
 * Powers up the CHESTER-X2 UART bridge for Modbus communication.
 * Should be called before any Modbus operations.
 *
 * @return 0 on success, negative error code on failure
 */
int app_modbus_enable(void);

/**
 * @brief Disable Modbus interface (power down SC16IS740)
 *
 * Powers down the CHESTER-X2 UART bridge to save power.
 * Should be called after Modbus operations are complete.
 *
 * @return 0 on success, negative error code on failure
 */
int app_modbus_disable(void);

/**
 * @brief Read Modbus holding registers
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param reg_addr Starting register address
 * @param count Number of registers to read
 * @param data Buffer to store read values
 * @return 0 on success, negative error code on failure
 */
int app_modbus_read_holding_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
				  uint16_t *data);

/**
 * @brief Read Modbus input registers
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param reg_addr Starting register address
 * @param count Number of registers to read
 * @param data Buffer to store read values
 * @return 0 on success, negative error code on failure
 */
int app_modbus_read_input_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
				uint16_t *data);

/**
 * @brief Write Modbus single holding register
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param reg_addr Register address
 * @param data Value to write
 * @return 0 on success, negative error code on failure
 */
int app_modbus_write_holding_reg(uint8_t slave_addr, uint16_t reg_addr, uint16_t data);

/**
 * @brief Write Modbus multiple holding registers
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param reg_addr Starting register address
 * @param count Number of registers to write
 * @param data Buffer containing values to write
 * @return 0 on success, negative error code on failure
 */
int app_modbus_write_holding_regs(uint8_t slave_addr, uint16_t reg_addr, uint16_t count,
				   const uint16_t *data);

/**
 * @brief Read Modbus coils
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param coil_addr Starting coil address
 * @param count Number of coils to read
 * @param data Buffer to store read coil states (bit-packed)
 * @return 0 on success, negative error code on failure
 */
int app_modbus_read_coils(uint8_t slave_addr, uint16_t coil_addr, uint16_t count, uint8_t *data);

/**
 * @brief Write Modbus single coil
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param coil_addr Coil address
 * @param value Coil value (true/false)
 * @return 0 on success, negative error code on failure
 */
int app_modbus_write_coil(uint8_t slave_addr, uint16_t coil_addr, bool value);

/**
 * @brief Sample all configured devices via Modbus
 *
 * Iterates through all configured devices in app_config.devices[] array
 * and reads data from each (Sensecap, Cubic, Lambrecht, etc.).
 *
 * @return 0 on success, negative error code on failure
 */
int app_modbus_sample(void);

/* Shell command handlers */

/**
 * @brief Shell command to read Modbus registers
 *
 * Usage: modbus read <slave_addr> <reg_addr> <count> [holding|input]
 */
int cmd_modbus_read(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to write Modbus register
 *
 * Usage: modbus write <slave_addr> <reg_addr> <value>
 */
int cmd_modbus_write(const struct shell *shell, size_t argc, char **argv);

/**
 * @brief Shell command to sample configured device
 *
 * Usage: modbus sample
 */
int cmd_modbus_sample(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_H_ */
