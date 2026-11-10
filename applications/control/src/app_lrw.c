/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */
#include "app_lrw.h"
#include "app_config.h"
#include "app_data.h"
#include "drivers/drv_ld81.h"

#include <chester/ctr_encode_lrw.h>
#include <chester/ctr_power.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/*
 * Local override of the SDK's CTR_ENCODE_LRW_W1_THERM: g_app_data.w1_therm.sensor
 * is now a heap-allocated array of length sensor_count rather than a fixed
 * APP_DATA_W1_THERM_COUNT array. Copied from chester/ctr_encode_lrw.h; intended
 * to be upstreamed to the SDK (and the other apps) later.
 */
#undef CTR_ENCODE_LRW_W1_THERM
#define CTR_ENCODE_LRW_W1_THERM(buf)                                                               \
	do {                                                                                       \
		float t[APP_DATA_W1_THERM_MAX_COUNT];                                              \
		int count = 0;                                                                     \
		for (size_t i = 0; i < g_app_data.w1_therm.sensor_count; i++) {                    \
			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];  \
			if (!sensor->serial_number) {                                              \
				continue;                                                          \
			}                                                                          \
			t[count++] = sensor->last_sample_temperature;                              \
		}                                                                                  \
		ret |= ctr_buf_append_u8(buf, count);                                              \
		for (size_t i = 0; i < count; i++) {                                               \
			if (isnan(t[i])) {                                                         \
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                   \
			} else {                                                                   \
				ret |= ctr_buf_append_s16_le(buf, t[i] * 100.f);                   \
			}                                                                          \
		}                                                                                  \
	} while (0)

LOG_MODULE_REGISTER(app_lrw, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_LRW)

/* Channels per X0 module: the X0-A block carries channels 1-4, the X0-B block 5-8. */
#define X0_MODULE_CHANNELS 4

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
/* The X0 block for channels [first, first + count): one mode byte per channel, followed
 * by that mode's payload. Local instead of the SDK's CTR_ENCODE_LRW_X0, so the shared
 * macro does not depend on this app's `power` channel mode.
 *
 * `power` marks a channel driven by the sensor layer, which is not sampled here, so no
 * payload follows its mode byte. It is encoded explicitly rather than left as
 * `disabled` so a consumer of this block alone can tell a sensor-driven channel from a
 * genuinely unused one; the readings themselves are reported in the SENSORS group.
 * Called with app data locked. */
static int encode_x0(struct ctr_buf *buf, int first, int count)
{
	int ret = 0;

	for (int i = first; i < first + count; i++) {
		if (g_app_data.trigger[i]) {
			ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_TRIGGER);
			int val = 0;
			val = g_app_data.trigger[i]->trigger_active ? 0x01 : 0x00;
			if (g_app_data.trigger[i]->event_count) {
				val |= g_app_data.trigger[i]->events[0].is_active ? 0x02 : 0x04;
			}
			ret |= ctr_buf_append_u8(buf, val);
		} else if (g_app_data.counter[i]) {
			ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_COUNTER);
			ret |= ctr_buf_append_u32_le(buf, g_app_data.counter[i]->value > UINT32_MAX
								  ? UINT32_MAX
								  : g_app_data.counter[i]->value);
			ret |= ctr_buf_append_u16_le(buf, g_app_data.counter[i]->delta > UINT16_MAX
								  ? UINT16_MAX
								  : g_app_data.counter[i]->delta);
		} else if (g_app_data.voltage[i]) {
			ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_VOLTAGE);
			if (isnan(g_app_data.voltage[i]->last_sample)) {
				ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
			} else {
				ret |= ctr_buf_append_u16_le(
					buf, g_app_data.voltage[i]->last_sample * 100.f);
			}
		} else if (g_app_data.current[i]) {
			ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_CURRENT);
			if (isnan(g_app_data.current[i]->last_sample)) {
				ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
			} else {
				ret |= ctr_buf_append_u16_le(
					buf, g_app_data.current[i]->last_sample * 100.f);
			}
		} else if (g_app_data.power[i]) {
			ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_POWER);
		} else {
			ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_DISABLED);
		}
	}

	return ret;
}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

/* Encodes the sections selected in `header`. Fails with a non-zero value when the
 * buffer (the 51-byte LoRaWAN MTU) cannot hold them. Called with app data locked. */
static int encode(struct ctr_buf *buf, uint8_t header)
{
	int ret = 0;

	ctr_buf_reset(buf);

	ret |= ctr_buf_append_u8(buf, header);

#if defined(FEATURE_SUBSYSTEM_POWER)
	if (header & BIT(0)) {
		ret |= ctr_power_lrw_encode(buf);
	}
#endif

#if defined(CONFIG_CTR_ACCEL)
	if (header & BIT(1)) {
		CTR_ENCODE_LRW_ACCEL(buf);
	}
#endif

#if defined(CONFIG_CTR_THERM)
	if (header & BIT(2)) {
		CTR_ENCODE_LRW_THERM(buf);
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	if (header & BIT(3)) {
		CTR_ENCODE_LRW_S2(buf);
	}
#endif

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	if (header & BIT(4)) {
		CTR_ENCODE_LRW_W1_THERM(buf);
	}
#endif

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	if (header & BIT(5)) {
		CTR_ENCODE_LRW_BLE_TAG(buf);
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	if (header & BIT(6)) {
		ret |= encode_x0(buf, 0, X0_MODULE_CHANNELS);
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	if (header & BIT(7)) {
		ret |= encode_x0(buf, X0_MODULE_CHANNELS, X0_MODULE_CHANNELS);
	}
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	/* Pluggable SENSORS group, appended after the X0 blocks. Each sensor is a
	 * self-contained block: a small header (type + status) followed by a
	 * length-prefixed, type-specific data sub-block, so the data for one
	 * sensor stays grouped and a decoder can skip unknown types by length:
	 *
	 *   [u8 count]
	 *   per sensor: [u8 type][u8 status][u8 data_len][data_len B]
	 *   status: bit0 = valid, bits 4-5 = slot (0-based, sensor-N is slot N-1)
	 *   LD-81 data (3 B): u16 LE voltage_mV (0xFFFF=invalid) + u8 flooded(0/1)
	 *
	 * It rides on the X0-A header bit, the header byte having no bit of its own
	 * left, so the decoder reads it only when that bit is set.
	 */
	if (header & BIT(6)) {
		uint8_t sensor_count = 0;

		for (int slot = 0; slot < APP_SENSORS_MAX; slot++) {
			if (g_app_data.sensors[slot].configured) {
				sensor_count++;
			}
		}
		ret |= ctr_buf_append_u8(buf, sensor_count);

		for (int slot = 0; slot < APP_SENSORS_MAX; slot++) {
			struct app_data_sensor *s = &g_app_data.sensors[slot];
			if (!s->configured) {
				continue;
			}

			uint8_t status = (slot & 0x03) << 4;

			ret |= ctr_buf_append_u8(buf, s->type_id);

			switch (s->type_id) {
			case APP_SENSOR_TYPE_LD81: {
				/* One snapshot, so status, millivolts and flood state in this
				 * frame cannot come from different samples. */
				struct app_data_ld81 d;
				bool valid = ld81_get_data(slot, &d) == 0 && d.valid;

				status |= valid ? BIT(0) : 0;
				ret |= ctr_buf_append_u8(buf, status);
				ret |= ctr_buf_append_u8(buf, 3); /* data_len */
				ret |= ctr_buf_append_u16_le(buf, valid ? (uint16_t)d.voltage_mv
									: 0xFFFF);
				ret |= ctr_buf_append_u8(buf, (valid && d.flooded) ? 1 : 0);
				break;
			}
			default:
				ret |= ctr_buf_append_u8(buf, status);
				ret |= ctr_buf_append_u8(buf, 0); /* data_len: no data */
				break;
			}
		}
	}
#endif

	return ret;
}

/* Encodes the sections selected in `*header`, giving up optional ones until the frame
 * fits. `*header` is left as sent. Fails with a non-zero value when the frame does not
 * fit even with every optional section dropped. Called with app data locked. */
static int encode_fit(struct ctr_buf *buf, uint8_t *header)
{
	/* Optional sections in the order they are given up when the payload does not
	 * fit the 51-byte MTU. The X0 blocks and the SENSORS group are never dropped
	 * here. The decoder follows the header bits, so a frame without some sections
	 * still decodes. */
	static const uint8_t drop_order[] = {BIT(5), BIT(4), BIT(3), BIT(1), BIT(2), BIT(0)};

	int ret = encode(buf, *header);

	for (size_t i = 0; ret && i < ARRAY_SIZE(drop_order); i++) {
		if (!(*header & drop_order[i])) {
			continue;
		}

		*header &= ~drop_order[i];
		ret = encode(buf, *header);
	}

	return ret;
}

int app_lrw_encode(struct ctr_buf *buf, struct ctr_buf *buf_b)
{
	uint8_t header = 0;

	header |= IS_ENABLED(FEATURE_SUBSYSTEM_POWER) ? BIT(0) : 0;
	header |= IS_ENABLED(CONFIG_CTR_ACCEL) ? BIT(1) : 0;
	header |= IS_ENABLED(CONFIG_CTR_THERM) ? BIT(2) : 0;
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_S2) ? BIT(3) : 0;
	header |= IS_ENABLED(FEATURE_SUBSYSTEM_DS18B20) ? BIT(4) : 0;
	header |= IS_ENABLED(FEATURE_SUBSYSTEM_BLE_TAG) ? BIT(5) : 0;
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_X0_A) ? BIT(6) : 0;
	header |= IS_ENABLED(FEATURE_HARDWARE_CHESTER_X0_B) ? BIT(7) : 0;

	uint8_t full = header;
	int ret;

	ctr_buf_reset(buf_b);

	app_data_lock();

	ret = encode_fit(buf, &header);

#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	/* With eight channels the X0 blocks and the SENSORS group alone can exceed the
	 * 51-byte MTU: an LD-81 next to six counters needs 54 B (header 1 + X0 46 +
	 * SENSORS 7), eight counters 57 B. Rather than send nothing, flood alarm
	 * included, channels 5-8 move to a second uplink of their own. The first keeps
	 * the SENSORS group, so an alarm leaves in it. The second is a frame whose header
	 * has only the X0-B bit set, so the same decoder reads both. Without X0-B every
	 * configuration fits one frame. */
	if (ret) {
		full &= ~BIT(7);
		header = full;
		ret = encode_fit(buf, &header);
		if (!ret) {
			ret = encode(buf_b, BIT(7));
		}
	}
#endif

	app_data_unlock();

	if (ret) {
		ctr_buf_reset(buf_b);
		LOG_ERR("Payload does not fit the LoRaWAN MTU");
		return -E2BIG;
	}

	if (header != full) {
		LOG_WRN("Payload too large; sent a reduced frame (header 0x%02x)", header);
	}

	if (ctr_buf_get_used(buf_b)) {
		LOG_INF("Payload too large for one frame; channels 5-8 follow in a second uplink");
	}

	return 0;
}
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */
