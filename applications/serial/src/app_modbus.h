/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MODBUS_H_
#define APP_MODBUS_H_

#include "app_modbus_addr.h"
#include "app_modbus_decode.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/devicetree.h>

/**
 * @brief Compile-time check: is any UART bridge shield (CTR-X2 RS-485 or
 *        CTR-X12 RS-232) present in the device tree?
 *
 * Use to skip Modbus initialization entirely on builds without a shield.
 */
#define APP_MODBUS_HW_PRESENT                                                                      \
	(DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x2_sc16is740_a), okay) ||                             \
	 DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_x12_sc16is740_a), okay))

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
 * @brief Check whether the Modbus client was successfully initialized
 *
 * The SC16IS740 UART bridge is shared with app_serial. Callers must not drive
 * Modbus traffic - or its PM resume/suspend bracketing - unless Modbus actually
 * owns the bus this boot.
 *
 * @return true if app_modbus_init() completed successfully
 */
bool app_modbus_is_ready(void);

/**
 * @brief Enable Modbus interface (power up SC16IS740)
 *
 * Powers up the CHESTER-X2 UART bridge for Modbus communication.
 * Should be called before any Modbus operations. Calls are reference-counted: the
 * bridge is powered down only by the app_modbus_disable() matching the first enable.
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
 * @brief Take exclusive ownership of the Modbus bus and power it up
 *
 * Held for a whole multi-register operation (a device sample, an address change, a
 * shell command) so that the work queue and the shell never interleave transactions
 * or power the bridge down under each other. Recursive for the owning thread.
 *
 * @return 0 on success, negative error code on failure (the bus is not held then)
 */
int app_modbus_acquire(void);

/**
 * @brief Power the bus down (if no one else enabled it) and release ownership
 */
void app_modbus_release(void);

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
 * @brief Read registers with the given function code
 *
 * @param slave_addr Modbus slave address (1-247)
 * @param reg_addr Starting register address
 * @param fc APP_MODBUS_FC_HOLDING (FC03) or APP_MODBUS_FC_INPUT (FC04)
 * @param count Number of registers to read
 * @param data Buffer to store read values
 * @return 0 on success, negative error code on failure
 */
int app_modbus_read_regs(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			 uint16_t count, uint16_t *data);

/* Typed readers of a single value; 32-bit values take the word order of the device */
int app_modbus_read_u16(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			uint16_t *out);
int app_modbus_read_s16(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc, int16_t *out);
int app_modbus_read_u32(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			enum app_modbus_wo wo, uint32_t *out);
int app_modbus_read_s32(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			enum app_modbus_wo wo, int32_t *out);
int app_modbus_read_float(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			  enum app_modbus_wo wo, float *out);

/**
 * @brief Read one value of the given format and convert it to float (no scaling)
 *
 * @return 0 on success, negative error code on failure
 */
int app_modbus_read_value(uint8_t slave_addr, uint16_t reg_addr, enum app_modbus_fc fc,
			  enum app_modbus_format fmt, float *out);

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
 * @brief Change the Modbus address of a device, checked by reading it back
 *
 * Holds the bus for the whole change, see app_modbus_addr_change().
 *
 * @param slave_addr Current address of the device
 * @param new_addr New address (1-247)
 * @param reg_addr Holding register that holds the address
 * @param result APPLIED or PENDING on success
 * @return 0 on success, negative error code on failure
 */
int app_modbus_change_addr(uint8_t slave_addr, uint8_t new_addr, uint16_t reg_addr,
			   enum app_modbus_addr_result *result);

/**
 * @brief Change the Modbus address of an Orno OR-WE-504 energy meter
 *
 * Unlocks the meter, writes the address 8 s later and holds the bus for the whole change
 * (about 10 s), see app_modbus_addr_change_or_we_504().
 *
 * @param slave_addr Current address of the meter
 * @param new_addr New address (1-247)
 * @return 0 when the meter answers at the new address, negative error code on failure
 */
int app_modbus_change_addr_or_we_504(uint8_t slave_addr, uint8_t new_addr);

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
 * @brief Sample the configured devices that have no device driver
 *
 * Only the Lambrecht and generic stubs; devices with a driver are sampled by
 * app_device_sample_all().
 *
 * @return 0 on success or if there is no such device, negative error code on failure
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
 * @brief Change the Modbus address of a device and print the outcome
 *
 * For the address commands of the device drivers, see app_modbus_change_addr().
 *
 * @return 0 on success, negative error code on failure (printed)
 */
int app_modbus_shell_change_addr(const struct shell *shell, uint8_t slave_addr, uint8_t new_addr,
				 uint16_t reg_addr);

/**
 * @brief Change the Modbus address of an OR-WE-504 and print the outcome
 *
 * See app_modbus_change_addr_or_we_504().
 *
 * @return 0 on success, negative error code on failure (printed)
 */
int app_modbus_shell_change_addr_or_we_504(const struct shell *shell, uint8_t slave_addr,
					   uint8_t new_addr);

/**
 * @brief Shell command to sample the configured devices without a driver
 *
 * Usage: modbus sample
 */
int cmd_modbus_sample(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_H_ */
