/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MODBUS_DECODE_H_
#define APP_MODBUS_DECODE_H_

/**
 * @file app_modbus_decode.h
 * @brief Decoding of multi-register Modbus values
 *
 * Pure functions without any Zephyr dependency, shared by app_modbus.c and the device
 * drivers (and unit-tested on their own).
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Modbus function code used to read registers */
enum app_modbus_fc {
	APP_MODBUS_FC_HOLDING = 0, /**< FC03 Read Holding Registers */
	APP_MODBUS_FC_INPUT = 1,   /**< FC04 Read Input Registers */
};

/** Order of the 16-bit words of a 32/64-bit value */
enum app_modbus_wo {
	APP_MODBUS_WO_HI_FIRST = 0, /**< Most significant word in the lowest register */
	APP_MODBUS_WO_LO_FIRST = 1, /**< Least significant word in the lowest register */
};

/** Register format of a single value (type and word order) */
enum app_modbus_format {
	APP_MODBUS_U16 = 0,
	APP_MODBUS_S16,
	APP_MODBUS_U32_HI,
	APP_MODBUS_U32_LO,
	APP_MODBUS_S32_HI,
	APP_MODBUS_S32_LO,
	APP_MODBUS_FLOAT_HI,
	APP_MODBUS_FLOAT_LO,
};

/** Number of 16-bit registers occupied by a value of the given format */
static inline int app_modbus_format_regs(enum app_modbus_format fmt)
{
	return fmt <= APP_MODBUS_S16 ? 1 : 2;
}

static inline uint32_t app_modbus_decode_u32(const uint16_t *regs, enum app_modbus_wo wo)
{
	if (wo == APP_MODBUS_WO_LO_FIRST) {
		return ((uint32_t)regs[1] << 16) | regs[0];
	}

	return ((uint32_t)regs[0] << 16) | regs[1];
}

static inline int32_t app_modbus_decode_s32(const uint16_t *regs, enum app_modbus_wo wo)
{
	return (int32_t)app_modbus_decode_u32(regs, wo);
}

static inline float app_modbus_decode_float(const uint16_t *regs, enum app_modbus_wo wo)
{
	uint32_t u = app_modbus_decode_u32(regs, wo);
	float f;

	memcpy(&f, &u, sizeof(f));

	return f;
}

static inline int64_t app_modbus_decode_s64(const uint16_t *regs, enum app_modbus_wo wo)
{
	uint64_t u;

	if (wo == APP_MODBUS_WO_LO_FIRST) {
		u = ((uint64_t)regs[3] << 48) | ((uint64_t)regs[2] << 32) |
		    ((uint64_t)regs[1] << 16) | regs[0];
	} else {
		u = ((uint64_t)regs[0] << 48) | ((uint64_t)regs[1] << 32) |
		    ((uint64_t)regs[2] << 16) | regs[3];
	}

	return (int64_t)u;
}

/**
 * @brief Decode one value of the given format to float
 *
 * @param regs Registers as read from the device (1 or 2, see app_modbus_format_regs)
 * @param fmt Register format
 * @return Decoded value (no scaling applied)
 */
static inline float app_modbus_decode_value(const uint16_t *regs, enum app_modbus_format fmt)
{
	switch (fmt) {
	case APP_MODBUS_U16:
		return (float)regs[0];
	case APP_MODBUS_S16:
		return (float)(int16_t)regs[0];
	case APP_MODBUS_U32_HI:
		return (float)app_modbus_decode_u32(regs, APP_MODBUS_WO_HI_FIRST);
	case APP_MODBUS_U32_LO:
		return (float)app_modbus_decode_u32(regs, APP_MODBUS_WO_LO_FIRST);
	case APP_MODBUS_S32_HI:
		return (float)app_modbus_decode_s32(regs, APP_MODBUS_WO_HI_FIRST);
	case APP_MODBUS_S32_LO:
		return (float)app_modbus_decode_s32(regs, APP_MODBUS_WO_LO_FIRST);
	case APP_MODBUS_FLOAT_HI:
		return app_modbus_decode_float(regs, APP_MODBUS_WO_HI_FIRST);
	case APP_MODBUS_FLOAT_LO:
		return app_modbus_decode_float(regs, APP_MODBUS_WO_LO_FIRST);
	default:
		return NAN;
	}
}

#ifdef __cplusplus
}
#endif

#endif /* APP_MODBUS_DECODE_H_ */
