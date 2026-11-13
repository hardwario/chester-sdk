/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Tests for the Modbus register decoders.
 */

#include "app_modbus_decode.h"

#include <zephyr/ztest.h>

#include <math.h>
#include <stdint.h>

ZTEST(modbus_decode, test_u32_s32)
{
	const uint16_t regs[2] = {0x1234, 0x5678};
	const uint16_t neg[2] = {0xFFFF, 0xFFFE};

	zassert_equal(app_modbus_decode_u32(regs, APP_MODBUS_WO_HI_FIRST), 0x12345678);
	zassert_equal(app_modbus_decode_u32(regs, APP_MODBUS_WO_LO_FIRST), 0x56781234);
	zassert_equal(app_modbus_decode_s32(neg, APP_MODBUS_WO_HI_FIRST), -2);
	zassert_equal(
		app_modbus_decode_s32((const uint16_t[]){0xFFFE, 0xFFFF}, APP_MODBUS_WO_LO_FIRST),
		-2);
}

ZTEST(modbus_decode, test_float)
{
	/* 1.5f = 0x3FC00000 */
	const uint16_t hi[2] = {0x3FC0, 0x0000};
	const uint16_t lo[2] = {0x0000, 0x3FC0};

	zassert_equal(app_modbus_decode_float(hi, APP_MODBUS_WO_HI_FIRST), 1.5f);
	zassert_equal(app_modbus_decode_float(lo, APP_MODBUS_WO_LO_FIRST), 1.5f);
}

ZTEST(modbus_decode, test_s64)
{
	const uint16_t hi[4] = {0x0000, 0x0000, 0x0001, 0x86A0};
	const uint16_t lo[4] = {0x86A0, 0x0001, 0x0000, 0x0000};
	const uint16_t neg[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

	zassert_equal(app_modbus_decode_s64(hi, APP_MODBUS_WO_HI_FIRST), 100000);
	zassert_equal(app_modbus_decode_s64(lo, APP_MODBUS_WO_LO_FIRST), 100000);
	zassert_equal(app_modbus_decode_s64(neg, APP_MODBUS_WO_HI_FIRST), -1);
}

ZTEST(modbus_decode, test_value)
{
	const uint16_t one[1] = {0xFF38};         /* -200 as s16, 65336 as u16 */
	const uint16_t two[2] = {0xFFFF, 0xFC18}; /* -1000 as s32 hi-first */

	zassert_equal(app_modbus_format_regs(APP_MODBUS_U16), 1);
	zassert_equal(app_modbus_format_regs(APP_MODBUS_S16), 1);
	zassert_equal(app_modbus_format_regs(APP_MODBUS_FLOAT_LO), 2);

	zassert_equal(app_modbus_decode_value(one, APP_MODBUS_U16), 65336.f);
	zassert_equal(app_modbus_decode_value(one, APP_MODBUS_S16), -200.f);
	zassert_equal(app_modbus_decode_value(two, APP_MODBUS_S32_HI), -1000.f);
	zassert_equal(app_modbus_decode_value(two, APP_MODBUS_U32_HI), 4294966296.f);
	zassert_equal(
		app_modbus_decode_value((const uint16_t[]){0xFC18, 0xFFFF}, APP_MODBUS_S32_LO),
		-1000.f);
	zassert_equal(
		app_modbus_decode_value((const uint16_t[]){0x0000, 0x3FC0}, APP_MODBUS_FLOAT_LO),
		1.5f);
	zassert_true(isnan(app_modbus_decode_value(two, (enum app_modbus_format)99)));
}

/* Register values of the OR-WE-504 table (drv_or_we_504.c): a raw value is decoded by its
 * format and divided by the divisor of the table. The same raw 0xFFFF is 65535 unsigned but -1
 * signed, so exported power must stay negative.
 */
ZTEST(modbus_decode, test_or_we_504_registers)
{
	const uint16_t voltage[1] = {2301};
	const uint16_t all_ones[1] = {0xFFFF};
	const uint16_t power[1] = {1000};
	const uint16_t s16_min[1] = {0x8000};
	const uint16_t energy[2] = {0x0001, 0x0002};
	const uint16_t energy_max[2] = {0xABCD, 0xEF01};

	zassert_within(app_modbus_decode_value(voltage, APP_MODBUS_U16) / 10.f, 230.1f, 0.001f);
	zassert_within(app_modbus_decode_value(all_ones, APP_MODBUS_U16) / 10.f, 6553.5f, 0.01f);
	zassert_within(app_modbus_decode_value(power, APP_MODBUS_S16) / 1000.f, 1.f, 0.0001f);
	zassert_within(app_modbus_decode_value(all_ones, APP_MODBUS_S16) / 1000.f, -0.001f,
		       0.0001f);
	zassert_within(app_modbus_decode_value(s16_min, APP_MODBUS_S16), -32768.f, 0.5f);
	zassert_equal(app_modbus_decode_u32(energy, APP_MODBUS_WO_HI_FIRST), 0x00010002);
	zassert_equal(app_modbus_decode_u32(energy_max, APP_MODBUS_WO_HI_FIRST), 0xABCDEF01);
}

ZTEST_SUITE(modbus_decode, NULL, NULL, NULL, NULL, NULL);
