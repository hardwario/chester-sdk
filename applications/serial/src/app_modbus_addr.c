/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_modbus_addr.h"

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <string.h>

/* Set in the function code of a reply that carries a Modbus exception */
#define FC_EXCEPTION 0x80

/* OR-WE-504: unlock (proprietary), write multiple registers, the voltage register */
#define OR_WE_504_FC_UNLOCK   0x28
#define OR_WE_504_FC_WRITE    0x10
#define OR_WE_504_REG_VOLTAGE 0x0000

/* The meter takes the address write within about 10 s after the unlock; a PC tool wrote it
 * after 8 s on hardware, and let the meter settle before it read at the new address */
#define OR_WE_504_UNLOCK_WAIT_MS 8000
#define OR_WE_504_SETTLE_MS      200

/* The client got a frame: a value, a Modbus exception (positive code) or a reply it rejected
 * (another unit ID, a CRC or length error) */
static bool is_reply(int ret)
{
	return ret >= 0 || ret == -EIO || ret == -EMSGSIZE || ret == -EINVAL;
}

/* 0 if no device answers at addr */
static int check_free(const struct app_modbus_addr_bus *bus, uint8_t addr, uint16_t reg, int tries)
{
	for (int i = 0; i < tries; i++) {
		uint16_t value;
		int ret = bus->read_reg(addr, reg, &value, bus->user);

		/* Any reply, an exception included, means another device has the address; any
		 * other error than no reply means the bus does not work (-ENODEV: Modbus not
		 * running) */
		if (ret != -ETIMEDOUT) {
			return is_reply(ret) ? -EADDRINUSE : ret;
		}
	}

	return 0;
}

static bool reads_back(const struct app_modbus_addr_bus *bus, uint8_t addr, uint16_t reg,
		       uint16_t expected, int tries)
{
	for (int i = 0; i < tries; i++) {
		uint16_t value = 0;

		if (bus->read_reg(addr, reg, &value, bus->user) == 0) {
			return value == expected;
		}
	}

	return false;
}

/* A value or a Modbus exception */
static bool answers(const struct app_modbus_addr_bus *bus, uint8_t addr, uint16_t reg, int tries)
{
	for (int i = 0; i < tries; i++) {
		uint16_t value;

		if (bus->read_reg(addr, reg, &value, bus->user) >= 0) {
			return true;
		}
	}

	return false;
}

int app_modbus_addr_change(const struct app_modbus_addr_bus *bus, uint8_t addr, uint8_t new_addr,
			   uint16_t reg, enum app_modbus_addr_result *result)
{
	int ret;

	if (new_addr == addr) {
		return -EINVAL;
	}

	ret = check_free(bus, new_addr, reg, 1);
	if (ret) {
		return ret;
	}

	ret = bus->write_reg(addr, reg, new_addr, bus->user);
	if (ret == 0) {
		/* Confirmed from the old address; the client checks the echoed value */
		*result = APP_MODBUS_ADDR_PENDING;
		return 0;
	}

	/* A reply from the new address fails with -EIO: look where the device answers now,
	 * twice, so that one disturbed frame does not turn a done change into an error */
	if (reads_back(bus, new_addr, reg, new_addr, 2)) {
		*result = APP_MODBUS_ADDR_APPLIED;
		return 0;
	}

	/* The reply was unreadable or lost, but the device may have stored the address */
	if (ret < 0 && reads_back(bus, addr, reg, new_addr, 1)) {
		*result = APP_MODBUS_ADDR_PENDING;
		return 0;
	}

	return ret < 0 ? ret : -EIO;
}

/* Function 0x28 to address 0xFE01, 2 registers, 4 bytes: the password 00000000 */
static int or_we_504_unlock(const struct app_modbus_addr_bus *bus, uint8_t addr)
{
	const struct app_modbus_addr_frame req = {
		.unit_id = addr,
		.fc = OR_WE_504_FC_UNLOCK,
		.len = 9,
		.data = {0xFE, 0x01, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00},
	};
	struct app_modbus_addr_frame rsp;
	int ret;

	ret = bus->transact(&req, &rsp, bus->user);
	if (ret) {
		return ret;
	}

	if (rsp.unit_id == addr && rsp.fc == (OR_WE_504_FC_UNLOCK | FC_EXCEPTION)) {
		return -EACCES;
	}

	/* The reply starts with the address 0xFE01 (its count is 1, not 2) */
	if (rsp.unit_id != addr || rsp.fc != OR_WE_504_FC_UNLOCK || rsp.len < 2 ||
	    rsp.data[0] != 0xFE || rsp.data[1] != 0x01) {
		return -EIO;
	}

	return 0;
}

int app_modbus_addr_change_or_we_504(const struct app_modbus_addr_bus *bus, uint8_t addr,
				     uint8_t new_addr)
{
	/* Register 0x000F, 1 register, 2 bytes: the address */
	const struct app_modbus_addr_frame req = {
		.unit_id = addr,
		.fc = OR_WE_504_FC_WRITE,
		.len = 7,
		.data = {0x00, 0x0F, 0x00, 0x01, 0x02, 0x00, new_addr},
	};
	struct app_modbus_addr_frame rsp;
	int ret;

	if (new_addr == addr) {
		return -EINVAL;
	}

	/* Twice: a device at new_addr that misses one request must not end up sharing it */
	ret = check_free(bus, new_addr, OR_WE_504_REG_VOLTAGE, 2);
	if (ret) {
		return ret;
	}

	/* The meter misses a request now and then; another unlock restarts the window */
	ret = or_we_504_unlock(bus, addr);
	if (ret && ret != -EACCES) {
		ret = or_we_504_unlock(bus, addr);
	}

	if (ret) {
		return ret;
	}

	bus->sleep(OR_WE_504_UNLOCK_WAIT_MS, bus->user);

	ret = bus->transact(&req, &rsp, bus->user);
	if (ret == 0 && rsp.unit_id == addr && rsp.fc == (OR_WE_504_FC_WRITE | FC_EXCEPTION)) {
		return -EACCES;
	}

	/* The echo of the register and count from the new address: no other device answers
	 * there, it was free */
	if (ret == 0 && rsp.unit_id == new_addr && rsp.fc == OR_WE_504_FC_WRITE && rsp.len == 4 &&
	    memcmp(rsp.data, req.data, 4) == 0) {
		return 0;
	}

	/* The reply was lost, unreadable or came from the old address: look at the new one */
	bus->sleep(OR_WE_504_SETTLE_MS, bus->user);

	return answers(bus, new_addr, OR_WE_504_REG_VOLTAGE, 2) ? 0 : -EIO;
}
