/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_lrw.h"
#include "app_config.h"
#include "app_data.h"
#include "app_device.h"
#include "app_driver_store.h"
#include "drivers/drv_interface.h"

/* Device drivers */
#include "drivers/drv_microsens_180hs.h"
#include "drivers/drv_em1xx.h"
#include "drivers/drv_em5xx.h"
#include "drivers/drv_or_we_504.h"
#include "drivers/drv_or_we_516.h"
#include "drivers/drv_iem3000.h"
#include "drivers/drv_promag_mf7s.h"
#include "drivers/drv_flowt_ft201.h"
#include "drivers/drv_piketronic_rpp.h"
#include "drivers/drv_piketronic_rppu.h"
#include "drivers/drv_solax_g3.h"

#include <chester/ctr_buf.h>
#include <chester/ctr_encode_lrw.h>
#include <chester/ctr_power.h>

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
 * LRW Payload Structure v2 (Float16 for RS-485):
 *
 * Strategy: as many devices per message as fit the MTU, the rest in follow-up messages
 *
 * Header (1 byte):
 *   BIT(7-4): Protocol version (2)
 *   BIT(3-0): Device count in this message
 *
 * System Data (8 bytes):
 *   Battery: 5B (CHESTER standard encoding)
 *   Accelerometer: 1B (orientation)
 *   Thermometer: 2B (INT16, /100 C)
 *
 * Device Header (4 bytes), repeated per device:
 *   [type:u8]   - Device type enum
 *   [addr:u8]   - Modbus address
 *   [status:u8] - Status flags (0x01=valid, 0x02=error)
 *   [len:u8]    - Length of the device data that follows, so a decoder can skip
 *                 a type it does not know and still decode the devices after it
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
 *   Piketronic RPP-R (4×Float16 = 8 bytes):
 *     radon 1 h avg, radon 1 day avg, temperature, humidity
 *
 *   Piketronic RPP-U (5×Float16 = 10 bytes):
 *     radon 0.5 h avg, radon 1 day avg, temperature, humidity, accumulator voltage
 *
 *   MicroSENS 180-HS (3×INT16 = 6 bytes, UNCHANGED):
 *     [co2:s16]   - CO2 Vol.-% * 10 (0x7FFF = invalid)
 *     [temp:s16]  - Temperature C * 10 (0x7FFF = invalid)
 *     [press:u16] - Pressure mbar (0xFFFF = invalid)
 *
 *   Table-driven devices: one value per item, see encode_items()
 *
 * Size of one device block (header + data):
 *   EM111/OR-WE-504:  4 + 12 = 16 bytes
 *   EM530/OR-WE-516:  4 + 30 = 34 bytes
 *   MicroSENS:        4 +  6 = 10 bytes
 *   Piketronic RPP-R: 4 +  8 = 12 bytes
 *   Piketronic RPP-U: 4 + 10 = 14 bytes
 * plus 9 bytes of header and system data per message.
 */

/*
 * Table-driven devices (item table, see app_driver_store.h): one value per item, in table
 * order, from the last closed aggregation window (or the last sample before the first
 * window closes). STATS / CIRCULAR send the average, LAST / COUNTER the value. The item's
 * lrw field selects the encoding of value * lrw_mul:
 *   F16 - Float16 LE, NaN if not available (default; COUNTER: U32 of value * mul)
 *   U16 - unsigned 16-bit LE, 0xFFFF if not available or out of range
 *   U32 - unsigned 32-bit LE, 0xFFFFFFFF if not available or out of range
 */
static int encode_items(struct ctr_buf *buf, const struct app_driver_slot *slot)
{
	const struct app_device_driver *drv = slot->drv;
	bool closed = app_driver_win_count(slot) > 0;
	uint32_t seq = app_driver_win_end(slot) - 1;
	int ret = 0;

	for (size_t i = 0; i < drv->item_count; i++) {
		const struct app_driver_item *item = &drv->items[i];
		enum app_driver_lrw lrw = item->lrw;
		float mul = item->lrw_mul;
		float value = closed ? app_driver_win_value(slot, i, seq)
				     : app_driver_items_last(slot, i);

		if (lrw == APP_DRIVER_LRW_AUTO) {
			bool counter = item->mode == APP_DRIVER_AGGREG_COUNTER;

			lrw = counter ? APP_DRIVER_LRW_U32 : APP_DRIVER_LRW_F16;
			mul = counter ? item->mul : 1.f;
		}

		double v = (double)value * (double)(mul != 0.f ? mul : 1.f);

		switch (lrw) {
		case APP_DRIVER_LRW_U16:
			ret |= ctr_buf_append_u16_le(buf, isnan(v) || v < 0. || v >= UINT16_MAX
								  ? UINT16_MAX
								  : (uint16_t)lround(v));
			break;
		case APP_DRIVER_LRW_U32:
			ret |= ctr_buf_append_u32_le(buf, isnan(v) || v < 0. || v >= UINT32_MAX
								  ? UINT32_MAX
								  : (uint32_t)llround(v));
			break;
		default:
			ret |= encode_float16_le(buf, (float)v);
			break;
		}
	}

	return ret;
}

/* MicroSENS encoding macro (6 bytes INT16 - UNCHANGED) */
#define ENCODE_MICROSENS(buf)                                                                      \
	do {                                                                                       \
		const struct app_data_microsens *m = microsens_get_data(slot);                     \
		if (!isnan(m->co2_percent) && !isinf(m->co2_percent) && m->valid) {                \
			ret |= ctr_buf_append_s16_le(buf, (int16_t)(m->co2_percent * 10.0f));      \
		} else {                                                                           \
			ret |= ctr_buf_append_s16_le(buf, INT16_MAX);                              \
		}                                                                                  \
		if (!isnan(m->temperature_c) && !isinf(m->temperature_c) && m->valid) {            \
			ret |= ctr_buf_append_s16_le(buf, (int16_t)(m->temperature_c * 10.0f));    \
		} else {                                                                           \
			ret |= ctr_buf_append_s16_le(buf, INT16_MAX);                              \
		}                                                                                  \
		if (!isnan(m->pressure_mbar) && !isinf(m->pressure_mbar) && m->valid) {            \
			ret |= ctr_buf_append_u16_le(buf, (uint16_t)m->pressure_mbar);             \
		} else {                                                                           \
			ret |= ctr_buf_append_u16_le(buf, UINT16_MAX);                             \
		}                                                                                  \
	} while (0)

/* EM111 single-phase encoding macro (6×Float16 = 12 bytes) */
#define ENCODE_EM1XX_FLOAT16(buf)                                                                  \
	do {                                                                                       \
		const struct app_data_em1xx *d = em1xx_get_data(slot);                             \
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
		const struct app_data_em5xx *d = em5xx_get_data(slot);                             \
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
		/* Totals */                                                                       \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_in : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* OR-WE-504 single-phase encoding macro (6×Float16 = 12 bytes) */
#define ENCODE_OR_WE_504_FLOAT16(buf)                                                              \
	do {                                                                                       \
		const struct app_data_or_we_504 *d = or_we_504_get_data(slot);                     \
		ret |= encode_float16_le(buf, d->valid ? d->voltage : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->current : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		float energy_kwh = d->valid ? (d->energy_in / 1000.0f) : NAN;                      \
		ret |= encode_float16_le(buf, energy_kwh); /* Energy in */                         \
		ret |= encode_float16_le(buf, 0.0f);       /* Energy out (not supported) */        \
	} while (0)

/* OR-WE-516 three-phase encoding macro (15×Float16 = 30 bytes) */
#define ENCODE_OR_WE_516_FLOAT16(buf)                                                              \
	do {                                                                                       \
		const struct app_data_or_we_516 *d = or_we_516_get_data(slot);                     \
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
		/* Totals */                                                                       \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_in : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* iEM3000 three-phase encoding macro (13×Float16 = 26 bytes) */
#define ENCODE_IEM3000_FLOAT16(buf)                                                                \
	do {                                                                                       \
		const struct app_data_iem3000 *d = iem3000_get_data(slot);                         \
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
		/* Totals */                                                                       \
		ret |= encode_float16_le(buf, d->valid ? d->power : NAN);                          \
		ret |= encode_float16_le(buf, d->valid ? d->frequency : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_in : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->energy_out : NAN);                     \
	} while (0)

/* Promag MF7S RFID encoding macro (4B UID + 4B timestamp + 2B counter = 10 bytes) */
#define ENCODE_PROMAG_MF7S(buf)                                                                    \
	do {                                                                                       \
		const struct app_data_promag_mf7s *d = promag_mf7s_get_data(slot);                 \
		uint32_t uid = d->valid ? d->last_uid : 0;                                         \
		ret |= ctr_buf_append_u8(buf, (uid >> 24) & 0xFF);                                 \
		ret |= ctr_buf_append_u8(buf, (uid >> 16) & 0xFF);                                 \
		ret |= ctr_buf_append_u8(buf, (uid >> 8) & 0xFF);                                  \
		ret |= ctr_buf_append_u8(buf, uid & 0xFF);                                         \
		uint32_t ts = d->valid ? d->last_read_ts : 0;                                      \
		ret |= ctr_buf_append_u8(buf, ts & 0xFF);                                          \
		ret |= ctr_buf_append_u8(buf, (ts >> 8) & 0xFF);                                   \
		ret |= ctr_buf_append_u8(buf, (ts >> 16) & 0xFF);                                  \
		ret |= ctr_buf_append_u8(buf, (ts >> 24) & 0xFF);                                  \
		ret |= ctr_buf_append_u16_le(buf, (uint16_t)(d->total_reads & 0xFFFF));            \
	} while (0)

/* FlowT FT201 encoding macro (7×Float16 = 14 bytes) */
#define ENCODE_FLOWT_FT201_FLOAT16(buf)                                                            \
	do {                                                                                       \
		const struct app_data_flowt_ft201 *d = flowt_ft201_get_data(slot);                 \
		ret |= encode_float16_le(buf, d->valid ? d->flow_h : NAN);                         \
		ret |= encode_float16_le(buf, d->valid ? d->velocity : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->temp_inlet : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->temp_outlet : NAN);                    \
		ret |= encode_float16_le(buf, d->valid ? d->energy_hot : NAN);                     \
		ret |= encode_float16_le(buf, d->valid ? d->energy_cold : NAN);                    \
		ret |= ctr_buf_append_u8(buf, d->valid ? d->signal_quality : 0xFF);                \
	} while (0)

/* Piketronic RPP-R radon probe encoding macro (4×Float16 = 8 bytes) */
#define ENCODE_PIKETRONIC_RPP_FLOAT16(buf)                                                         \
	do {                                                                                       \
		const struct app_data_piketronic_rpp *d = piketronic_rpp_get_data(slot);           \
		ret |= encode_float16_le(buf, d->valid ? (float)d->concentration : NAN);           \
		ret |= encode_float16_le(buf, d->valid ? (float)d->concentration_day : NAN);       \
		ret |= encode_float16_le(buf, d->valid ? (float)d->temperature : NAN);             \
		ret |= encode_float16_le(buf, d->valid ? (float)d->humidity : NAN);                \
	} while (0)

/* Piketronic RPP-U radon probe encoding macro (5xFloat16 = 10 bytes)
 *
 * The first four fields match the RPP-R layout slot-for-slot, though the probes differ: the
 * RPP-R's first field is a 1 h average, the RPP-U's is a 0.5 h average. The fifth
 * is the accumulator voltage in volts, which the RPP-R has no equivalent for because it is
 * bus-powered. Float16 near 3.7 resolves to about 2 mV - far finer than the 3300 mV
 * power-off threshold this field exists to warn about. Because the layout now diverges from
 * the RPP-R, the LRW decoder needs its own RPP-U function rather than sharing one.
 */
#define ENCODE_PIKETRONIC_RPPU_FLOAT16(buf)                                                        \
	do {                                                                                       \
		const struct drv_piketronic_rppu_data *d = piketronic_rppu_get_data(slot);         \
		ret |= encode_float16_le(buf, d->valid ? (float)d->concentration_ac : NAN);        \
		ret |= encode_float16_le(buf, d->valid ? (float)d->concentration_day_ac : NAN);    \
		ret |= encode_float16_le(buf, d->valid ? (float)d->temperature : NAN);             \
		ret |= encode_float16_le(buf, d->valid ? (float)d->humidity : NAN);                \
		ret |= encode_float16_le(buf, d->valid ? (float)d->voltage_mv / 1000.f : NAN);     \
	} while (0)

/* SolaX X3-Hybrid G3 customer set (11×Float16 = 22 bytes) */
#define ENCODE_SOLAX_G3_FLOAT16(buf)                                                               \
	do {                                                                                       \
		const struct app_data_solax_g3 *d = solax_g3_get_data(slot);                       \
		ret |= encode_float16_le(buf, d->valid ? d->pv1_power : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->pv2_power : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->bat_power : NAN);                      \
		ret |= encode_float16_le(buf, d->valid ? d->bat_temp : NAN);                       \
		ret |= encode_float16_le(buf, d->valid ? d->bat_soc : NAN);                        \
		ret |= encode_float16_le(buf, d->valid ? d->feedin_power : NAN);                   \
		ret |= encode_float16_le(buf, d->valid ? d->feedin_energy_total : NAN);            \
		ret |= encode_float16_le(buf, d->valid ? d->consume_energy_total : NAN);           \
		ret |= encode_float16_le(buf, d->valid ? d->eps_power_l1 : NAN);                   \
		ret |= encode_float16_le(buf, d->valid ? d->eps_power_l2 : NAN);                   \
		ret |= encode_float16_le(buf, d->valid ? d->eps_power_l3 : NAN);                   \
	} while (0)

/* Pack as many configured devices as the MTU allows, starting at @p start_idx.
 *
 * Previously each device got its own uplink, which for a three-probe RPP-R installation meant
 * three transmissions carrying the same 9 bytes of system data three times, each separated by
 * the stack's 8 s intra-request delay. Three RPP-R devices fit in 45 of the 51 available bytes,
 * so one uplink does the job at a third of the airtime and duty-cycle cost.
 *
 * Device blocks vary from 10 to 34 bytes, so eight devices cannot always fit. Rather than
 * computing sizes per type up front, a device is encoded and then rolled back if it pushed the
 * payload past the MTU; the caller then starts a fresh message from where this one stopped.
 * That needs a buffer larger than the MTU to overshoot into - see app_send.c.
 *
 * @param buf         Output buffer, must be larger than APP_LRW_MTU
 * @param start_idx   First device index to consider
 * @param next_idx    Set to the index to resume from, or APP_CONFIG_MAX_DEVICES when done
 * @param packed_mask Set to BIT(idx) of every device in the message (NULL = not needed)
 * @return Number of devices packed, or a negative errno
 */
int app_lrw_encode_multi(struct ctr_buf *buf, int start_idx, int *next_idx, uint8_t *packed_mask)
{
	int ret = 0;
	int packed = 0;

	if (start_idx < 0 || start_idx >= APP_CONFIG_MAX_DEVICES || next_idx == NULL) {
		LOG_ERR("Invalid arguments: start_idx=%d", start_idx);
		return -EINVAL;
	}

	if (packed_mask) {
		*packed_mask = 0;
	}

	ctr_buf_reset(buf);

	app_data_lock();

	/* Header: [version:4bit][device_count:4bit]. Version 2 carries a count where version 1
	 * carried a single device index, so a v1 payload can never be misread as a v2 one.
	 */
	ret |= ctr_buf_append_u8(buf, (APP_LRW_VERSION << 4));

	/* System data (8 bytes) */
#if defined(FEATURE_SUBSYSTEM_POWER)
	ret |= ctr_power_lrw_encode(buf); /* 5 bytes */
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

	int idx = start_idx;

	for (; idx < APP_CONFIG_MAX_DEVICES; idx++) {
		struct app_config_device *dev = &g_app_config.devices[idx];

		if (dev->type == APP_DEVICE_TYPE_NONE) {
			continue;
		}

		/* No slot: a type without a driver, or a device that could not be set up */
		struct app_driver_slot *slot = app_device_get_slot(idx);

		if (!slot) {
			LOG_WRN("Device %d has no active driver, skipping", idx);
			continue;
		}

		/* Roll-back point: if this device does not fit, the payload is truncated back
		 * to here and the caller resumes from this index in a fresh message.
		 */
		size_t mark = ctr_buf_get_used(buf);
		int dev_ret = 0;

		dev_ret |= ctr_buf_append_u8(buf, (uint8_t)dev->type);
		dev_ret |= ctr_buf_append_u8(buf, dev->addr);
		dev_ret |= ctr_buf_append_u8(buf, 0x01); /* status: 0x01=valid, 0x02=error */

		/* Data length, patched below. It lets a decoder that does not know this
		 * type skip it and still decode the devices after it. */
		size_t len_pos = ctr_buf_get_used(buf);

		dev_ret |= ctr_buf_append_u8(buf, 0);

		app_driver_slot_lock(slot);

		if (slot->drv->items) {
			ret |= encode_items(buf, slot);
			goto encoded;
		}

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
			ENCODE_PROMAG_MF7S(buf); /* 10 bytes */
			break;
		case APP_DEVICE_TYPE_FLOWT_FT201:
			ENCODE_FLOWT_FT201_FLOAT16(buf); /* 13 bytes */
			break;
		case APP_DEVICE_TYPE_PIKETRONIC_RPP:
			ENCODE_PIKETRONIC_RPP_FLOAT16(buf); /* 8 bytes: 4xFloat16 */
			break;
		case APP_DEVICE_TYPE_PIKETRONIC_RPPU:
			ENCODE_PIKETRONIC_RPPU_FLOAT16(buf); /* 10 bytes: 5xFloat16 */
			break;
		case APP_DEVICE_TYPE_SOLAX_G3:
			ENCODE_SOLAX_G3_FLOAT16(buf); /* 22 bytes: 11xFloat16 */
			break;
		default:
			app_driver_slot_unlock(slot);
			LOG_WRN("Unknown device type %d at index %d, skipping", dev->type, idx);
			ctr_buf_seek(buf, mark);
			continue;
		}

encoded:
		app_driver_slot_unlock(slot);

		/* `ret` is shared with the ENCODE_* macros, so fold it in and reset it: a buffer
		 * error raised while encoding this device must not condemn the whole payload.
		 */
		dev_ret |= ret;
		ret = 0;

		if (dev_ret || ctr_buf_get_used(buf) > APP_LRW_MTU) {
			ctr_buf_seek(buf, mark);

			if (packed == 0) {
				/* A single device that cannot fit on its own is a real problem,
				 * not something a follow-up message can rescue.
				 */
				LOG_ERR("Device %d (type %d) does not fit in a %d-byte payload",
					idx, dev->type, APP_LRW_MTU);
				app_data_unlock();
				*next_idx = idx + 1;
				return -E2BIG;
			}

			LOG_DBG("Device %d deferred to the next message", idx);
			break;
		}

		ctr_buf_get_mem(buf)[len_pos] = (uint8_t)(ctr_buf_get_used(buf) - len_pos - 1);

		if (packed_mask) {
			*packed_mask |= BIT(idx);
		}

		packed++;
		LOG_DBG("Packed device %d: type=%d, addr=%u, size now %zu", idx, dev->type,
			dev->addr, ctr_buf_get_used(buf));

		if (packed == 0x0F) {
			/* The count is a nibble. */
			idx++;
			break;
		}
	}

	app_data_unlock();

	*next_idx = idx;

	if (packed == 0) {
		return 0;
	}

	/* Patch the device count into the header now that it is known. */
	ctr_buf_get_mem(buf)[0] = (APP_LRW_VERSION << 4) | (packed & 0x0F);

	size_t payload_size = ctr_buf_get_used(buf);
	LOG_INF("LRW payload: %zu bytes (%d device(s), v%d)", payload_size, packed,
		APP_LRW_VERSION);

	/* The exact bytes that go on air. Without this the only way to check an encoder change
	 * is to wait for the payload to surface in the network server, which needs a working
	 * join - so a keyless bench board could never verify its own encoding.
	 *
	 * At DBG so it stays quiet in the field. CONFIG_LOG_RUNTIME_FILTERING is enabled, so
	 * nothing is lost by that: `log enable dbg app_lrw` at the shell brings it back live,
	 * with no rebuild and no reflash.
	 */
	LOG_HEXDUMP_DBG(ctr_buf_get_mem(buf), payload_size, "LRW payload bytes");

	return packed;
}

/* Convenience wrapper: encode everything that fits in one message, from the first device. */
int app_lrw_encode(struct ctr_buf *buf)
{
	int next_idx = APP_CONFIG_MAX_DEVICES;
	int packed = app_lrw_encode_multi(buf, 0, &next_idx, NULL);

	if (packed < 0) {
		return packed;
	}

	if (packed == 0) {
		LOG_WRN("No active devices to encode");
		return -ENODEV;
	}

	if (next_idx < APP_CONFIG_MAX_DEVICES) {
		LOG_WRN("%d device(s) did not fit and are not sent by this wrapper; use "
			"app_lrw_encode_multi() to send the remainder",
			APP_CONFIG_MAX_DEVICES - next_idx);
	}

	return 0;
}
