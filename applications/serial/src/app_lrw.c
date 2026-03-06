/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_lrw.h"
#include "app_config.h"
#include "app_data.h"
#include "app_device.h"

/* Device drivers */
#include "drivers/drv_microsens_180hs.h"
#include "drivers/drv_em1xx.h"
#include "drivers/drv_em5xx.h"
#include "drivers/drv_or_we_504.h"
#include "drivers/drv_or_we_516.h"
#include "drivers/drv_iem3000.h"
#include "drivers/drv_promag_mf7s.h"
#include "drivers/drv_flowt_ft201.h"

#include <chester/ctr_buf.h>
#include <chester/ctr_encode_lrw.h>

#include <zcbor_common.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(app_lrw, LOG_LEVEL_DBG);

static inline int encode_float16_le(struct ctr_buf *buf, float value)
{
	uint16_t f16 = zcbor_float32_to_16(value);
	return ctr_buf_append_u16_le(buf, f16);
}

/*
 * Single-Device LRW Payload Structure (Float16 for RS-485):
 *
 * Strategy: ONE device per message
 *
 * Header (1 byte):
 *   BIT(7-4): Protocol version (1 = Float16)
 *   BIT(3-0): Device index (0-7)
 *
 * System Data (8 bytes):
 *   Battery: 5B (CHESTER standard encoding)
 *   Accelerometer: 1B (orientation)
 *   Thermometer: 2B (INT16, /100 C)
 *
 * Device Header (3 bytes):
 *   [type:u8]   - Device type enum
 *   [addr:u8]   - Modbus address
 *   [status:u8] - Status flags (0x01=valid, 0x02=error)
 *
 * Device Data (Float16 for RS-485, INT16 for MicroSENS):
 *
 *   EM111 (6×Float16 = 12 bytes):
 *     voltage, current, power, frequency, energy_in, energy_out
 *
 *   EM530 (15×Float16 = 30 bytes):
 *     voltage_l1, voltage_l2, voltage_l3,
 *     current_l1, current_l2, current_l3,
 *     power_l1, power_l2, power_l3,
 *     power_total, frequency, energy_in, energy_out
 *
 *   OR-WE-504 (6×Float16 = 12 bytes):
 *     voltage, current, power, frequency, energy_in, energy_out
 *
 *   OR-WE-516 (15×Float16 = 30 bytes):
 *     Same as EM530
 *
 *   MicroSENS 180-HS (3×INT16 = 6 bytes, UNCHANGED):
 *     [co2:s16]   - CO2 Vol.-% * 10 (0x7FFF = invalid)
 *     [temp:s16]  - Temperature C * 10 (0x7FFF = invalid)
 *     [press:u16] - Pressure mbar (0xFFFF = invalid)
 *
 * Total sizes:
 *   EM111/OR-WE-504: 1 + 8 + 3 + 12 = 24 bytes
 *   EM530/OR-WE-516: 1 + 8 + 3 + 30 = 42 bytes
 *   MicroSENS:       1 + 8 + 3 +  6 = 18 bytes
 */

/* MicroSENS encoding macro (6 bytes INT16 - UNCHANGED) */
#define ENCODE_MICROSENS(buf)                                                                      \
	do {                                                                                       \
		const struct app_data_microsens *m = microsens_get_data();                         \
		if (!isnan(m->co2_percent) && !isinf(m->co2_percent) && m->valid) {               \
			ret |= ctr_buf_append_s16_le(buf, (int16_t)(m->co2_percent * 10.0f));     \
		} else {                                                                           \
			ret |= ctr_buf_append_s16_le(buf, INT16_MAX);                             \
		}                                                                                  \
		if (!isnan(m->temperature_c) && !isinf(m->temperature_c) && m->valid) {           \
			ret |= ctr_buf_append_s16_le(buf, (int16_t)(m->temperature_c * 10.0f));   \
		} else {                                                                           \
			ret |= ctr_buf_append_s16_le(buf, INT16_MAX);                             \
		}                                                                                  \
		if (!isnan(m->pressure_mbar) && !isinf(m->pressure_mbar) && m->valid) {           \
			ret |= ctr_buf_append_u16_le(buf, (uint16_t)m->pressure_mbar);            \
		} else {                                                                           \
			ret |= ctr_buf_append_u16_le(buf, UINT16_MAX);                            \
		}                                                                                  \
	} while (0)

/* EM111 single-phase encoding macro (6×Float16 = 12 bytes) */
#define ENCODE_EM1XX_FLOAT16(buf)                                                                  \
	do {                                                                                       \
		const struct app_data_em1xx *d = em1xx_get_data();                                 \
		ret |= encode_float16_le(buf, d->valid ? d->voltage : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->current : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_in : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* EM530 three-phase encoding macro (15×Float16 = 30 bytes) */
#define ENCODE_EM5XX_FLOAT16(buf)                                                                  \
	do {                                                                                       \
		const struct app_data_em5xx *d = em5xx_get_data();                                 \
		/* Per-phase voltages */                                                           \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l1 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l2 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l3 : NAN);                     \
		/* Per-phase currents */                                                           \
		ret |= encode_float16_le(buf, d->valid ? d->current_l1 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->current_l2 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->current_l3 : NAN);                     \
		/* Per-phase powers */                                                             \
		ret |= encode_float16_le(buf, d->valid ? d->power_l1 : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->power_l2 : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->power_l3 : NAN);                       \
		/* Totals */                                                                        \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_in : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* OR-WE-504 single-phase encoding macro (6×Float16 = 12 bytes) */
#define ENCODE_OR_WE_504_FLOAT16(buf)                                                              \
	do {                                                                                       \
		const struct app_data_or_we_504 *d = or_we_504_get_data();                        \
		ret |= encode_float16_le(buf, d->valid ? d->voltage : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->current : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? 50.0f : NAN); /* Frequency placeholder */ \
		float energy_kwh = d->valid ? (d->energy / 1000.0f) : NAN;                         \
		ret |= encode_float16_le(buf, energy_kwh); /* Energy in */                         \
		ret |= encode_float16_le(buf, 0.0f);       /* Energy out (not supported) */        \
	} while (0)

/* OR-WE-516 three-phase encoding macro (15×Float16 = 30 bytes) */
#define ENCODE_OR_WE_516_FLOAT16(buf)                                                              \
	do {                                                                                       \
		const struct app_data_or_we_516 *d = or_we_516_get_data();                        \
		/* Per-phase voltages */                                                           \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l1 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l2 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l3 : NAN);                     \
		/* Per-phase currents */                                                           \
		ret |= encode_float16_le(buf, d->valid ? d->current_l1 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->current_l2 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->current_l3 : NAN);                     \
		/* Per-phase powers */                                                             \
		ret |= encode_float16_le(buf, d->valid ? d->power_l1 : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->power_l2 : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->power_l3 : NAN);                       \
		/* Totals */                                                                        \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy : NAN);                         \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* iEM3000 three-phase encoding macro (13×Float16 = 26 bytes) */
#define ENCODE_IEM3000_FLOAT16(buf)                                                                \
	do {                                                                                       \
		const struct app_data_iem3000 *d = iem3000_get_data();                             \
		/* Per-phase voltages */                                                           \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l1 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l2 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage_l3 : NAN);                     \
		/* Per-phase currents */                                                           \
		ret |= encode_float16_le(buf, d->valid ? d->current_l1 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->current_l2 : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->current_l3 : NAN);                     \
		/* Per-phase powers */                                                             \
		ret |= encode_float16_le(buf, d->valid ? d->power_l1 : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->power_l2 : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->power_l3 : NAN);                       \
		/* Totals */                                                                        \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_in : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* Promag MF7S RFID encoding macro (4B UID + 4B timestamp + 2B counter = 10 bytes) */
#define ENCODE_PROMAG_MF7S(buf)                                                                    \
	do {                                                                                       \
		const struct app_data_promag_mf7s *d = promag_mf7s_get_data();                     \
		uint32_t uid = d->valid ? d->last_uid : 0;                                         \
		ret |= ctr_buf_append_u8(buf, (uid >> 24) & 0xFF);                                \
		ret |= ctr_buf_append_u8(buf, (uid >> 16) & 0xFF);                                \
		ret |= ctr_buf_append_u8(buf, (uid >> 8) & 0xFF);                                 \
		ret |= ctr_buf_append_u8(buf, uid & 0xFF);                                        \
		uint32_t ts = d->valid ? d->last_read_ts : 0;                                      \
		ret |= ctr_buf_append_u8(buf, ts & 0xFF);                                         \
		ret |= ctr_buf_append_u8(buf, (ts >> 8) & 0xFF);                                  \
		ret |= ctr_buf_append_u8(buf, (ts >> 16) & 0xFF);                                 \
		ret |= ctr_buf_append_u8(buf, (ts >> 24) & 0xFF);                                 \
		ret |= ctr_buf_append_u16_le(buf, (uint16_t)(d->total_reads & 0xFFFF));           \
	} while (0)

/* FlowT FT201 encoding macro (7×Float16 = 14 bytes) */
#define ENCODE_FLOWT_FT201_FLOAT16(buf)                                                            \
	do {                                                                                       \
		const struct app_data_flowt_ft201 *d = flowt_ft201_get_data();                    \
		ret |= encode_float16_le(buf, d->valid ? d->flow_h : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->velocity : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->temp_inlet : NAN);                    \
		ret |= encode_float16_le(buf, d->valid ? d->temp_outlet : NAN);                   \
		ret |= encode_float16_le(buf, d->valid ? d->energy_hot : NAN);                    \
		ret |= encode_float16_le(buf, d->valid ? d->energy_cold : NAN);                   \
		ret |= ctr_buf_append_u8(buf, d->valid ? d->signal_quality : 0xFF);              \
	} while (0)

/**
 * @brief Encode single device into LoRaWAN payload
 *
 * @param buf Output buffer (max 51 bytes)
 * @param device_idx Device index (0-7)
 * @return int 0 on success, negative error code on failure
 */
int app_lrw_encode_single(struct ctr_buf *buf, int device_idx)
{
	int ret = 0;

	if (device_idx < 0 || device_idx >= APP_CONFIG_MAX_DEVICES) {
		LOG_ERR("Invalid device index: %d", device_idx);
		return -EINVAL;
	}

	ctr_buf_reset(buf);

	app_data_lock();

	struct app_config_device *dev = &g_app_config.devices[device_idx];

	if (dev->type == APP_DEVICE_TYPE_NONE) {
		LOG_WRN("Device %d is not configured", device_idx);
		app_data_unlock();
		return -ENODEV;
	}

	/* Build header: [version:4bit][device_index:4bit] */
	uint8_t header = 0;
	header |= (0x01 << 4); /* Protocol version 1 (Float16) */
	header |= (device_idx & 0x0F);

	ret |= ctr_buf_append_u8(buf, header);

	LOG_DBG("Header: version=1, device_idx=%u", device_idx);

	/* System data (8 bytes) */
#if defined(CONFIG_CTR_BATT)
	CTR_ENCODE_LRW_BATTERY(buf); /* 5 bytes */
#else
	ret |= ctr_buf_append_u16_le(buf, 0);
	ret |= ctr_buf_append_u16_le(buf, 0);
	ret |= ctr_buf_append_u8(buf, 0);
#endif

#if defined(CONFIG_CTR_ACCEL)
	CTR_ENCODE_LRW_ACCEL(buf); /* 1 byte */
#else
	ret |= ctr_buf_append_u8(buf, 0);
#endif

#if defined(CONFIG_CTR_THERM)
	CTR_ENCODE_LRW_THERM(buf); /* 2 bytes */
#else
	ret |= ctr_buf_append_u16_le(buf, 0);
#endif

	LOG_DBG("System data encoded, buffer size: %zu", ctr_buf_get_used(buf));

	/* Device header (3 bytes) */
	ret |= ctr_buf_append_u8(buf, (uint8_t)dev->type);
	ret |= ctr_buf_append_u8(buf, dev->addr);

	/* Status flags: 0x01=valid, 0x02=error */
	uint8_t status = 0x01; /* Assume valid for now */
	ret |= ctr_buf_append_u8(buf, status);

	LOG_DBG("Encoding device %d: type=%d, addr=%u", device_idx, dev->type, dev->addr);

	/* Device-specific data */
	switch (dev->type) {
	case APP_DEVICE_TYPE_MICROSENS_180HS:
		ENCODE_MICROSENS(buf); /* 6 bytes INT16 */
		break;

	case APP_DEVICE_TYPE_EM1XX:
		ENCODE_EM1XX_FLOAT16(buf); /* 12 bytes Float16 */
		break;

	case APP_DEVICE_TYPE_EM5XX:
		ENCODE_EM5XX_FLOAT16(buf); /* 30 bytes Float16 */
		break;

	case APP_DEVICE_TYPE_OR_WE_504:
		ENCODE_OR_WE_504_FLOAT16(buf); /* 12 bytes Float16 */
		break;

	case APP_DEVICE_TYPE_OR_WE_516:
		ENCODE_OR_WE_516_FLOAT16(buf); /* 30 bytes Float16 */
		break;

	case APP_DEVICE_TYPE_IEM3000:
		ENCODE_IEM3000_FLOAT16(buf); /* 26 bytes Float16 */
		break;

	case APP_DEVICE_TYPE_PROMAG_MF7S:
		ENCODE_PROMAG_MF7S(buf); /* 10 bytes: 4B UID + 4B timestamp + 2B counter */
		break;

	case APP_DEVICE_TYPE_FLOWT_FT201:
		ENCODE_FLOWT_FT201_FLOAT16(buf); /* 13 bytes: 6×Float16 + 1B signal */
		break;

	default:
		LOG_WRN("Unknown device type: %d", dev->type);
		app_data_unlock();
		return -ENOTSUP;
	}

	app_data_unlock();

	if (ret) {
		LOG_ERR("Buffer encode failed");
		return -EFAULT;
	}

	size_t payload_size = ctr_buf_get_used(buf);
	LOG_INF("LRW payload: %zu bytes (device_idx=%u, type=%d)", payload_size, device_idx,
		dev->type);

	if (payload_size > 51) {
		LOG_ERR("Payload exceeds 51-byte MTU: %zu bytes", payload_size);
		return -E2BIG;
	}

	return 0;
}

/* Legacy wrapper for backward compatibility */
int app_lrw_encode(struct ctr_buf *buf)
{
	/* Encode first active device */
	for (int i = 0; i < APP_CONFIG_MAX_DEVICES; i++) {
		if (g_app_config.devices[i].type != APP_DEVICE_TYPE_NONE) {
			return app_lrw_encode_single(buf, i);
		}
	}

	LOG_WRN("No active devices to encode");
	return -ENODEV;
}
