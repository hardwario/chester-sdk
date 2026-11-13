/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MODBUS_ADDR_H_
#define APP_MODBUS_ADDR_H_

/**
 * @file app_modbus_addr.h
 * @brief Changing the Modbus address of a device, checked by reading it back
 *
 * Devices differ in when they take a new address. According to their manuals, the Cubic 6303
 * answers the write from the new address at once, which the Zephyr client rejects with -EIO
 * (a reply from another unit ID) although the change went through, and the SenseCAP stations
 * confirm the write from the old address and move after a power cycle.
 * app_modbus_addr_change() tells the two apart by reading the address register back, so it
 * copes with either behaviour.
 *
 * The Orno OR-WE-504 takes a new address only after an unlock with a proprietary function
 * code, and answers from the new address; app_modbus_addr_change_or_we_504() sends both
 * frames without the checks of the Zephyr client.
 *
 * The module has no dependency on the bus (callbacks), so it is unit-tested on its own.
 */

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Outcome of a successful address change */
enum app_modbus_addr_result {
	/** The device answers at the new address */
	APP_MODBUS_ADDR_APPLIED,
	/** The device stored the new address and takes it after a power cycle */
	APP_MODBUS_ADDR_PENDING,
};

/** Modbus RTU frame without the CRC */
struct app_modbus_addr_frame {
	uint8_t unit_id;
	uint8_t fc;
	/** Number of bytes in data */
	uint8_t len;
	uint8_t data[12];
};

/** Bus access of the address changes */
struct app_modbus_addr_bus {
	/** Read one holding register; -ETIMEDOUT when no device answers */
	int (*read_reg)(uint8_t addr, uint16_t reg, uint16_t *value, void *user);
	/** Write one holding register (FC06); a Modbus exception is a positive code */
	int (*write_reg)(uint8_t addr, uint16_t reg, uint16_t value, void *user);
	/** Send a frame and receive the reply as the device sent it, from any unit ID and with
	 *  any function code; -ETIMEDOUT when no device answers (OR-WE-504 only) */
	int (*transact)(const struct app_modbus_addr_frame *req, struct app_modbus_addr_frame *rsp,
			void *user);
	/** Wait (OR-WE-504 only) */
	void (*sleep)(int32_t ms, void *user);
	void *user;
};

/**
 * @brief Change the Modbus address of a device and find out when it takes effect
 *
 * Refuses an address another device already answers at, so the bus never ends up with
 * two devices at one address.
 *
 * @param bus Register access
 * @param addr Current address of the device
 * @param new_addr New address
 * @param reg Holding register that holds the address
 * @param result APPLIED or PENDING on success
 * @return 0 on success, -EINVAL if new_addr is addr, -EADDRINUSE if another device answers
 *         at new_addr, the bus error if the bus does not work (-ENODEV while Modbus is not
 *         running), another negative error code if the change failed
 */
int app_modbus_addr_change(const struct app_modbus_addr_bus *bus, uint8_t addr, uint8_t new_addr,
			   uint16_t reg, enum app_modbus_addr_result *result);

/**
 * @brief Change the Modbus address of an Orno OR-WE-504 energy meter
 *
 * The meter takes a new address only within about 10 s after an unlock with the proprietary
 * function 0x28 and its password (00000000 by default), and only by FC16 to register 0x000F.
 * It answers that write from the new address. The write goes 8 s after the unlock, as with a
 * PC tool that did it on hardware; earlier writes were never tried. The meter misses a request
 * now and then, so a missed unlock is sent once more, and a change not confirmed from the new
 * address is checked by reading the voltage register there. The address checks read the
 * voltage register too, since reading the address register was never tried.
 *
 * @param bus Bus access, with transact and sleep
 * @param addr Current address of the meter
 * @param new_addr New address
 * @return 0 when the meter answers at new_addr, -EINVAL if new_addr is addr, -EADDRINUSE if
 *         another device answers at new_addr, the bus error if the bus does not work (-ENODEV
 *         while Modbus is not running), -ETIMEDOUT if the meter does not answer the unlock,
 *         -EACCES if it refuses the unlock or the write (another password), -EIO if it does
 *         not answer at new_addr after the write
 */
int app_modbus_addr_change_or_we_504(const struct app_modbus_addr_bus *bus, uint8_t addr,
				     uint8_t new_addr);

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_ADDR_H_ */
