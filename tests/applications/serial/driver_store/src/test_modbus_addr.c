/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for changing a Modbus device address, with a fake bus of devices that keep their
 * address in a holding register, and of OR-WE-504 meters that take a new address only after
 * an unlock.
 */

#include "app_modbus_addr.h"

#include <zephyr/ztest.h>

#include <errno.h>
#include <string.h>

#define REG_CUBIC    2
#define REG_SENSECAP 0x1000

/* OR-WE-504: the register the change is checked at (voltage) and the function codes */
#define REG_OR_WE_504_VOLTAGE 0x0000
#define FC_WRITE_REGS         0x10
#define FC_UNLOCK             0x28
#define FC_EXCEPTION          0x80

/* Modbus exception codes, returned by the Zephyr client as a positive value */
#define EXC_ILLEGAL_FUNCTION     1
#define EXC_ILLEGAL_DATA_ADDRESS 2
#define EXC_ILLEGAL_DATA_VALUE   3

/* Bus time of an answered request and of a request without a reply */
#define FRAME_MS   50
#define TIMEOUT_MS 1000

struct fake_device {
	bool present;
	uint8_t addr;        /* address the device answers at */
	uint16_t reg;        /* holding register that holds the address */
	uint16_t stored;     /* value of that register */
	bool apply_on_write; /* takes the new address at once, not after a power cycle */
	bool reply_from_new; /* answers the address write from the new address */
	bool garble_reply;   /* stores the write, but its reply is unreadable */
	bool lose_reply;     /* stores the write, but its reply never arrives */
	int miss_reads;      /* reads the device does not answer */
	/* OR-WE-504, see fake_unlock() and fake_write_regs() */
	bool or_we_504;
	int64_t unlocked_at;    /* bus time of the last unlock, -1 if none */
	int miss_unlocks;       /* unlocks the meter misses */
	int bad_unlock_replies; /* unlocks answered with an unexpected frame, not taken */
	bool refuse_unlock;     /* another password is set: the unlock gets an exception */
	bool refuse_write;      /* the address write gets an exception */
	bool miss_write;        /* the meter misses the address write */
};

struct fake_bus {
	struct fake_device dev[2];
	int bus_error;     /* returned by every read and write when non-zero */
	int write_error;   /* returned by every write when non-zero, nothing stored */
	int disturb_reads; /* reads after the first write that fail with -EIO */
	int64_t now_ms;    /* bus time */
	int reads;
	int writes; /* FC06 writes and FC16 frames */
	int unlocks;
	uint8_t write_addr;
	uint16_t write_reg;
	uint16_t write_value;
	struct app_modbus_addr_frame unlock;      /* the last unlock frame */
	struct app_modbus_addr_frame write_frame; /* the last FC16 frame */
};

static struct fake_bus m_bus;

static struct fake_device cubic(uint8_t addr)
{
	return (struct fake_device){.present = true,
				    .addr = addr,
				    .reg = REG_CUBIC,
				    .stored = addr,
				    .apply_on_write = true,
				    .reply_from_new = true};
}

static struct fake_device sensecap(uint8_t addr)
{
	return (struct fake_device){
		.present = true, .addr = addr, .reg = REG_SENSECAP, .stored = addr};
}

static struct fake_device or_we_504(uint8_t addr)
{
	return (struct fake_device){.present = true,
				    .addr = addr,
				    .apply_on_write = true,
				    .reply_from_new = true,
				    .or_we_504 = true,
				    .unlocked_at = -1};
}

static struct fake_device *find(uint8_t addr)
{
	for (size_t i = 0; i < ARRAY_SIZE(m_bus.dev); i++) {
		if (m_bus.dev[i].present && m_bus.dev[i].addr == addr) {
			return &m_bus.dev[i];
		}
	}

	return NULL;
}

static int fake_read(uint8_t addr, uint16_t reg, uint16_t *value, void *user)
{
	struct fake_device *dev = find(addr);

	ARG_UNUSED(user);

	m_bus.reads++;

	if (m_bus.bus_error) {
		return m_bus.bus_error;
	}

	if (m_bus.writes > 0 && m_bus.disturb_reads > 0) {
		m_bus.disturb_reads--;
		return -EIO;
	}

	if (dev == NULL || dev->miss_reads > 0) {
		if (dev != NULL) {
			dev->miss_reads--;
		}

		m_bus.now_ms += TIMEOUT_MS;
		return -ETIMEDOUT;
	}

	m_bus.now_ms += FRAME_MS;

	/* Reading the OR-WE-504 address register was never tried on hardware */
	if (dev->or_we_504 ? reg != REG_OR_WE_504_VOLTAGE : reg != dev->reg) {
		return EXC_ILLEGAL_DATA_ADDRESS;
	}

	*value = dev->or_we_504 ? 2300 : dev->stored;
	return 0;
}

static int fake_write(uint8_t addr, uint16_t reg, uint16_t value, void *user)
{
	struct fake_device *dev = find(addr);

	ARG_UNUSED(user);

	m_bus.writes++;
	m_bus.write_addr = addr;
	m_bus.write_reg = reg;
	m_bus.write_value = value;

	if (m_bus.bus_error) {
		return m_bus.bus_error;
	}

	if (dev == NULL) {
		return -ETIMEDOUT;
	}

	if (m_bus.write_error) {
		return m_bus.write_error;
	}

	/* The OR-WE-504 takes its address only by FC16 after an unlock */
	if (dev->or_we_504 || reg != dev->reg) {
		return EXC_ILLEGAL_DATA_ADDRESS;
	}

	dev->stored = value;

	if (dev->apply_on_write) {
		dev->addr = (uint8_t)value;
	}

	if (dev->lose_reply) {
		return -ETIMEDOUT;
	}

	/* The Zephyr client rejects a reply from another unit ID with -EIO */
	return dev->reply_from_new || dev->garble_reply ? -EIO : 0;
}

static int reply(struct app_modbus_addr_frame *rsp, uint8_t unit_id, uint8_t fc,
		 const uint8_t *data, uint8_t len)
{
	rsp->unit_id = unit_id;
	rsp->fc = fc;
	rsp->len = len;
	memcpy(rsp->data, data, len);

	return 0;
}

static int exception(struct app_modbus_addr_frame *rsp, uint8_t unit_id, uint8_t fc, uint8_t code)
{
	return reply(rsp, unit_id, fc | FC_EXCEPTION, &code, 1);
}

/* The OR-WE-504 unlock: function 0x28, address 0xFE01, 2 registers, 4 bytes of password */
static int fake_unlock(struct fake_device *dev, const struct app_modbus_addr_frame *req,
		       struct app_modbus_addr_frame *rsp)
{
	static const uint8_t request[] = {0xFE, 0x01, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00};
	static const uint8_t answer[] = {0xFE, 0x01, 0x00, 0x01};
	static const uint8_t unexpected[] = {0x00, 0x00, 0x00, 0x00};

	if (dev->miss_unlocks > 0) {
		dev->miss_unlocks--;
		m_bus.now_ms += TIMEOUT_MS;
		return -ETIMEDOUT;
	}

	m_bus.now_ms += FRAME_MS;

	if (dev->refuse_unlock || req->len != sizeof(request) ||
	    memcmp(req->data, request, sizeof(request)) != 0) {
		return exception(rsp, dev->addr, req->fc, EXC_ILLEGAL_DATA_VALUE);
	}

	if (dev->bad_unlock_replies > 0) {
		dev->bad_unlock_replies--;
		return reply(rsp, dev->addr, req->fc, unexpected, sizeof(unexpected));
	}

	dev->unlocked_at = m_bus.now_ms;

	return reply(rsp, dev->addr, req->fc, answer, sizeof(answer));
}

/* The OR-WE-504 takes FC16 to its address register 0x000F within 10 s after an unlock. A PC
 * tool wrote it 8 s after the unlock on hardware and earlier writes were never tried, so the
 * fake takes it only from 8 s on. The meter answers from the new address. */
static int fake_write_regs(struct fake_device *dev, const struct app_modbus_addr_frame *req,
			   struct app_modbus_addr_frame *rsp)
{
	/* Register 0x000F, 1 register, 2 bytes, the high byte of the value */
	static const uint8_t header[] = {0x00, 0x0F, 0x00, 0x01, 0x02, 0x00};
	int64_t since_unlock = m_bus.now_ms - dev->unlocked_at;
	uint8_t old_addr = dev->addr;
	uint8_t new_addr;

	if (dev->miss_write) {
		m_bus.now_ms += TIMEOUT_MS;
		return -ETIMEDOUT;
	}

	m_bus.now_ms += FRAME_MS;

	if (dev->refuse_write || dev->unlocked_at < 0 || since_unlock < 8000 ||
	    since_unlock >= 10000 || req->len != sizeof(header) + 1 ||
	    memcmp(req->data, header, sizeof(header)) != 0) {
		return exception(rsp, dev->addr, req->fc, EXC_ILLEGAL_DATA_ADDRESS);
	}

	new_addr = req->data[sizeof(header)];
	dev->stored = new_addr;

	if (dev->apply_on_write) {
		dev->addr = new_addr;
	}

	if (dev->lose_reply) {
		m_bus.now_ms += TIMEOUT_MS - FRAME_MS;
		return -ETIMEDOUT;
	}

	if (dev->garble_reply) {
		return -EIO;
	}

	/* The echo of the register address and count */
	return reply(rsp, dev->reply_from_new ? new_addr : old_addr, req->fc, req->data, 4);
}

/* A frame without the checks of the Zephyr client: the reply comes as the device sends it */
static int fake_transact(const struct app_modbus_addr_frame *req, struct app_modbus_addr_frame *rsp,
			 void *user)
{
	struct fake_device *dev = find(req->unit_id);

	ARG_UNUSED(user);

	if (req->fc == FC_UNLOCK) {
		m_bus.unlocks++;
		m_bus.unlock = *req;
	} else {
		m_bus.writes++;
		m_bus.write_frame = *req;
	}

	if (m_bus.bus_error) {
		return m_bus.bus_error;
	}

	if (dev == NULL) {
		m_bus.now_ms += TIMEOUT_MS;
		return -ETIMEDOUT;
	}

	if (dev->or_we_504 && req->fc == FC_UNLOCK) {
		return fake_unlock(dev, req, rsp);
	}

	if (dev->or_we_504 && req->fc == FC_WRITE_REGS) {
		return fake_write_regs(dev, req, rsp);
	}

	m_bus.now_ms += FRAME_MS;

	return exception(rsp, dev->addr, req->fc, EXC_ILLEGAL_FUNCTION);
}

static void fake_sleep(int32_t ms, void *user)
{
	ARG_UNUSED(user);

	m_bus.now_ms += ms;
}

static const struct app_modbus_addr_bus m_fake = {
	.read_reg = fake_read,
	.write_reg = fake_write,
	.transact = fake_transact,
	.sleep = fake_sleep,
};

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	memset(&m_bus, 0, sizeof(m_bus));
}

ZTEST(modbus_addr, test_applied_from_new_address)
{
	enum app_modbus_addr_result result;

	/* Cubic: takes the new address at once and answers the write from it */
	m_bus.dev[0] = cubic(1);

	zassert_equal(app_modbus_addr_change(&m_fake, 1, 5, REG_CUBIC, &result), 0);
	zassert_equal(result, APP_MODBUS_ADDR_APPLIED);
	zassert_equal(m_bus.writes, 1);
	zassert_equal(m_bus.write_addr, 1);
	zassert_equal(m_bus.write_reg, REG_CUBIC);
	zassert_equal(m_bus.write_value, 5);
}

ZTEST(modbus_addr, test_applied_after_disturbed_read_back)
{
	enum app_modbus_addr_result result;

	/* One unreadable frame right after the change must not report a failure */
	m_bus.dev[0] = cubic(1);
	m_bus.disturb_reads = 1;

	zassert_equal(app_modbus_addr_change(&m_fake, 1, 5, REG_CUBIC, &result), 0);
	zassert_equal(result, APP_MODBUS_ADDR_APPLIED);
}

ZTEST(modbus_addr, test_pending_until_power_cycle)
{
	enum app_modbus_addr_result result;

	/* SenseCAP: confirms the write from the old address, moves after a power cycle */
	m_bus.dev[0] = sensecap(10);

	zassert_equal(app_modbus_addr_change(&m_fake, 10, 20, REG_SENSECAP, &result), 0);
	zassert_equal(result, APP_MODBUS_ADDR_PENDING);
	zassert_equal(m_bus.writes, 1);
	zassert_equal(m_bus.write_reg, REG_SENSECAP);
}

ZTEST(modbus_addr, test_pending_after_garbled_reply)
{
	enum app_modbus_addr_result result;

	/* The write was stored, the reply was unreadable, the device answers at the old one */
	m_bus.dev[0] = sensecap(10);
	m_bus.dev[0].garble_reply = true;

	zassert_equal(app_modbus_addr_change(&m_fake, 10, 20, REG_SENSECAP, &result), 0);
	zassert_equal(result, APP_MODBUS_ADDR_PENDING);
}

ZTEST(modbus_addr, test_pending_after_lost_reply)
{
	enum app_modbus_addr_result result;

	/* The write was stored, but its reply never arrived */
	m_bus.dev[0] = sensecap(10);
	m_bus.dev[0].lose_reply = true;

	zassert_equal(app_modbus_addr_change(&m_fake, 10, 20, REG_SENSECAP, &result), 0);
	zassert_equal(result, APP_MODBUS_ADDR_PENDING);
}

ZTEST(modbus_addr, test_new_address_taken)
{
	enum app_modbus_addr_result result;

	m_bus.dev[0] = sensecap(10);
	m_bus.dev[1] = sensecap(1);

	zassert_equal(app_modbus_addr_change(&m_fake, 10, 1, REG_SENSECAP, &result), -EADDRINUSE);
	zassert_equal(m_bus.writes, 0);
}

ZTEST(modbus_addr, test_exception_at_new_address)
{
	enum app_modbus_addr_result result;

	/* The Cubic at the new address has no register 0x1000 and answers with an exception */
	m_bus.dev[0] = sensecap(10);
	m_bus.dev[1] = cubic(1);

	zassert_equal(app_modbus_addr_change(&m_fake, 10, 1, REG_SENSECAP, &result), -EADDRINUSE);
	zassert_equal(m_bus.writes, 0);
}

ZTEST(modbus_addr, test_bus_not_running)
{
	enum app_modbus_addr_result result;

	/* The Zephyr client returns -ENODEV while Modbus is not initialized */
	m_bus.dev[0] = cubic(1);
	m_bus.bus_error = -ENODEV;

	zassert_equal(app_modbus_addr_change(&m_fake, 1, 5, REG_CUBIC, &result), -ENODEV);
	zassert_equal(m_bus.writes, 0);
}

ZTEST(modbus_addr, test_no_device)
{
	enum app_modbus_addr_result result;

	zassert_equal(app_modbus_addr_change(&m_fake, 1, 5, REG_CUBIC, &result), -ETIMEDOUT);
}

ZTEST(modbus_addr, test_write_exception)
{
	enum app_modbus_addr_result result;

	m_bus.dev[0] = cubic(1);
	m_bus.write_error = 3;

	zassert_equal(app_modbus_addr_change(&m_fake, 1, 5, REG_CUBIC, &result), -EIO);
}

ZTEST(modbus_addr, test_same_address)
{
	enum app_modbus_addr_result result;

	m_bus.dev[0] = cubic(1);

	zassert_equal(app_modbus_addr_change(&m_fake, 1, 1, REG_CUBIC, &result), -EINVAL);
	zassert_equal(m_bus.reads + m_bus.writes, 0);
}

ZTEST(modbus_addr, test_or_we_504_applied)
{
	/* The frames of the vendor example, for address 2 -> 10 */
	static const uint8_t unlock[] = {0xFE, 0x01, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00};
	static const uint8_t write[] = {0x00, 0x0F, 0x00, 0x01, 0x02, 0x00, 0x0A};

	/* Unlocked, written inside the window, answered from the new address */
	m_bus.dev[0] = or_we_504(2);

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), 0);
	zassert_equal(m_bus.dev[0].addr, 10);
	zassert_equal(m_bus.unlocks, 1);
	zassert_equal(m_bus.unlock.unit_id, 2);
	zassert_equal(m_bus.unlock.fc, 0x28);
	zassert_equal(m_bus.unlock.len, sizeof(unlock));
	zassert_mem_equal(m_bus.unlock.data, unlock, sizeof(unlock));
	zassert_equal(m_bus.writes, 1);
	zassert_equal(m_bus.write_frame.unit_id, 2);
	zassert_equal(m_bus.write_frame.fc, 0x10);
	zassert_equal(m_bus.write_frame.len, sizeof(write));
	zassert_mem_equal(m_bus.write_frame.data, write, sizeof(write));
}

ZTEST(modbus_addr, test_or_we_504_unlock_missed_once)
{
	/* The meter misses a request now and then: the second unlock opens the window */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].miss_unlocks = 1;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), 0);
	zassert_equal(m_bus.unlocks, 2);
	zassert_equal(m_bus.dev[0].addr, 10);
}

ZTEST(modbus_addr, test_or_we_504_unexpected_unlock_reply)
{
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].bad_unlock_replies = 1;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), 0);
	zassert_equal(m_bus.unlocks, 2);
	zassert_equal(m_bus.dev[0].addr, 10);
}

ZTEST(modbus_addr, test_or_we_504_write_reply_lost)
{
	/* The meter moved, but its reply from the new address never arrived */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].lose_reply = true;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), 0);
}

ZTEST(modbus_addr, test_or_we_504_check_disturbed)
{
	/* An unreadable reply and one disturbed check: the second check finds the meter */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].garble_reply = true;
	m_bus.disturb_reads = 1;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), 0);
}

ZTEST(modbus_addr, test_or_we_504_write_missed)
{
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].miss_write = true;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -EIO);
	zassert_equal(m_bus.dev[0].addr, 2);
}

ZTEST(modbus_addr, test_or_we_504_confirmed_from_old_address)
{
	/* An echo from the old address does not show that the meter moved */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].apply_on_write = false;
	m_bus.dev[0].reply_from_new = false;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -EIO);
}

ZTEST(modbus_addr, test_or_we_504_unlock_refused)
{
	/* Another password than the default 00000000 */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].refuse_unlock = true;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -EACCES);
	zassert_equal(m_bus.unlocks, 1);
	zassert_equal(m_bus.writes, 0);
}

ZTEST(modbus_addr, test_or_we_504_write_refused)
{
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[0].refuse_write = true;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -EACCES);
	zassert_equal(m_bus.dev[0].addr, 2);
}

ZTEST(modbus_addr, test_or_we_504_no_meter)
{
	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -ETIMEDOUT);
	zassert_equal(m_bus.unlocks, 2);
	zassert_equal(m_bus.writes, 0);
}

ZTEST(modbus_addr, test_or_we_504_new_address_taken)
{
	/* The Cubic at the new address answers the voltage read with an exception */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[1] = cubic(10);

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -EADDRINUSE);
	zassert_equal(m_bus.unlocks + m_bus.writes, 0);
}

ZTEST(modbus_addr, test_or_we_504_new_address_missed_once)
{
	/* A device at the new address that misses one request must not end up sharing it */
	m_bus.dev[0] = or_we_504(2);
	m_bus.dev[1] = or_we_504(10);
	m_bus.dev[1].miss_reads = 1;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -EADDRINUSE);
	zassert_equal(m_bus.unlocks + m_bus.writes, 0);
}

ZTEST(modbus_addr, test_or_we_504_bus_not_running)
{
	m_bus.dev[0] = or_we_504(2);
	m_bus.bus_error = -ENODEV;

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 10), -ENODEV);
	zassert_equal(m_bus.unlocks + m_bus.writes, 0);
}

ZTEST(modbus_addr, test_or_we_504_same_address)
{
	m_bus.dev[0] = or_we_504(2);

	zassert_equal(app_modbus_addr_change_or_we_504(&m_fake, 2, 2), -EINVAL);
	zassert_equal(m_bus.reads + m_bus.unlocks + m_bus.writes, 0);
}

ZTEST_SUITE(modbus_addr, NULL, NULL, before, NULL, NULL);
