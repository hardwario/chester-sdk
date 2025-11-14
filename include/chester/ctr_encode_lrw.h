/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_ENCODE_LRW_H_
#define CHESTER_INCLUDE_CTR_ENCODE_LRW_H_

#define CTR_ENCODE_LRW_BATTERY(buf)                                                                \
	do {                                                                                       \
		if (isnan(g_app_data.system_voltage_rest)) {                                       \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint16_t val = g_app_data.system_voltage_rest;                             \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(g_app_data.system_voltage_load)) {                                       \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint16_t val = g_app_data.system_voltage_load;                             \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(g_app_data.system_current_load)) {                                       \
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));                                \
		} else {                                                                           \
			uint8_t val = g_app_data.system_current_load;                              \
			ret |= ctr_buf_append_u8(buf, val);                                        \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_ACCEL(buf)                                                                  \
	do {                                                                                       \
		if (g_app_data.accel_orientation == INT_MAX) {                                     \
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));                                \
		} else {                                                                           \
			uint8_t val = g_app_data.accel_orientation;                                \
			ret |= ctr_buf_append_u8(buf, val);                                        \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_THERM(buf)                                                                  \
	do {                                                                                       \
		if (isnan(g_app_data.therm_temperature)) {                                         \
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                           \
		} else {                                                                           \
			int16_t val = g_app_data.therm_temperature * 100.f;                        \
			ret |= ctr_buf_append_s16_le(buf, val);                                    \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_S2(buf)                                                                     \
	do {                                                                                       \
		struct app_data_hygro *hygro = &g_app_data.hygro;                                  \
		if (isnan(hygro->last_sample_temperature)) {                                       \
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                           \
		} else {                                                                           \
			ret |= ctr_buf_append_s16_le(buf, hygro->last_sample_temperature * 100.f); \
		}                                                                                  \
		if (isnan(hygro->last_sample_humidity)) {                                          \
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));                                \
		} else {                                                                           \
			ret |= ctr_buf_append_u8(buf, hygro->last_sample_humidity * 2.f);          \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_W1_THERM(buf)                                                               \
	do {                                                                                       \
		float t[APP_DATA_W1_THERM_COUNT];                                                  \
		int count = 0;                                                                     \
		for (size_t i = 0; i < APP_DATA_W1_THERM_COUNT; i++) {                             \
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

#define CTR_ENCODE_LRW_RTD_THERM(buf)                                                              \
	do {                                                                                       \
		ret |= ctr_buf_append_u8(buf, APP_DATA_RTD_THERM_COUNT);                           \
		for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {                               \
			struct app_data_rtd_therm_sensor *sensor =                                 \
				&g_app_data.rtd_therm.sensor[i];                                   \
			if (isnan(sensor->last_sample_temperature)) {                              \
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                   \
			} else {                                                                   \
				ret |= ctr_buf_append_s16_le(                                      \
					buf, sensor->last_sample_temperature * 100.f);             \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_TC_THERM(buf)                                                               \
	do {                                                                                       \
		ret |= ctr_buf_append_u8(buf, APP_DATA_TC_THERM_COUNT);                            \
		for (int i = 0; i < APP_DATA_TC_THERM_COUNT; i++) {                                \
			struct app_data_tc_therm_sensor *sensor = &g_app_data.tc_therm.sensor[i];  \
			if (isnan(sensor->last_sample_temperature)) {                              \
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                   \
			} else {                                                                   \
				ret |= ctr_buf_append_s16_le(                                      \
					buf, sensor->last_sample_temperature * 100.f);             \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_BLE_TAG(buf)                                                                \
	do {                                                                                       \
		float temperature[CTR_BLE_TAG_COUNT];                                              \
		float humidity[CTR_BLE_TAG_COUNT];                                                 \
		int count = 0;                                                                     \
		for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {                                   \
			struct app_data_ble_tag_sensor *tag = &g_app_data.ble_tag.sensor[i];       \
			if (tag->addr[0] == 0 && tag->addr[1] == 0 && tag->addr[2] == 0 &&         \
			    tag->addr[3] == 0 && tag->addr[4] == 0 && tag->addr[5] == 0) {         \
				continue;                                                          \
			}                                                                          \
			temperature[count] = tag->last_sample_temperature;                         \
			humidity[count] = tag->last_sample_humidity;                               \
			count++;                                                                   \
		}                                                                                  \
		ret |= ctr_buf_append_u8(buf, count);                                              \
		for (size_t i = 0; i < count; i++) {                                               \
			if (isnan(temperature[i])) {                                               \
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                   \
			} else {                                                                   \
				ret |= ctr_buf_append_s16_le(buf, temperature[i] * 100.f);         \
			}                                                                          \
			if (isnan(humidity[i])) {                                                  \
				ret |= ctr_buf_append_u8(buf, BIT_MASK(8));                        \
			} else {                                                                   \
				ret |= ctr_buf_append_u8(buf, humidity[i] * 2.f);                  \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_SOIL_SENSOR(buf)                                                            \
	do {                                                                                       \
		int count = 0;                                                                     \
		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {                             \
			struct app_data_soil_sensor_sensor *sensor =                               \
				&g_app_data.soil_sensor.sensor[i];                                 \
			if (sensor->serial_number == 0) {                                          \
				continue;                                                          \
			}                                                                          \
			count++;                                                                   \
		}                                                                                  \
		ret |= ctr_buf_append_u8(buf, count);                                              \
		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {                             \
			struct app_data_soil_sensor_sensor *sensor =                               \
				&g_app_data.soil_sensor.sensor[i];                                 \
			if (sensor->serial_number == 0) {                                          \
				continue;                                                          \
			}                                                                          \
			if (isnan(sensor->last_temperature)) {                                     \
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                   \
			} else {                                                                   \
				ret |= ctr_buf_append_s16_le(buf,                                  \
							     sensor->last_temperature * 100.f);    \
			}                                                                          \
			if (isnan(sensor->last_moisture)) {                                        \
				ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                   \
			} else {                                                                   \
				ret |= ctr_buf_append_u16_le(buf, sensor->last_moisture);          \
			}                                                                          \
		}                                                                                  \
	} while (0)

#define CTR_ENCODE_LRW_X0(buf)                                                                     \
	do {                                                                                       \
		for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {                                  \
			if (g_app_data.trigger[i]) {                                               \
				ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_TRIGGER);    \
				int val = 0;                                                       \
				val = g_app_data.trigger[i]->trigger_active ? 0x01 : 0x00;         \
				if (g_app_data.trigger[i]->event_count) {                          \
					val |= g_app_data.trigger[i]->events[0].is_active ? 0x02   \
											  : 0x04;  \
				}                                                                  \
				ret |= ctr_buf_append_u8(buf, val);                                \
			}                                                                          \
			if (g_app_data.counter[i]) {                                               \
				ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_COUNTER);    \
				ret |= ctr_buf_append_u32_le(                                      \
					buf, g_app_data.counter[i]->value > UINT32_MAX             \
						     ? UINT32_MAX                                  \
						     : g_app_data.counter[i]->value);              \
				ret |= ctr_buf_append_u16_le(                                      \
					buf, g_app_data.counter[i]->delta > UINT16_MAX             \
						     ? UINT16_MAX                                  \
						     : g_app_data.counter[i]->delta);              \
			}                                                                          \
			if (g_app_data.voltage[i]) {                                               \
				ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_VOLTAGE);    \
				if (isnan(g_app_data.voltage[i]->last_sample)) {                   \
					ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));           \
				} else {                                                           \
					ret |= ctr_buf_append_u16_le(                              \
						buf, g_app_data.voltage[i]->last_sample * 100.f);  \
				}                                                                  \
			}                                                                          \
			if (g_app_data.current[i]) {                                               \
				ret |= ctr_buf_append_u8(buf, APP_CONFIG_CHANNEL_MODE_CURRENT);    \
				if (isnan(g_app_data.current[i]->last_sample)) {                   \
					ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));           \
				} else {                                                           \
					ret |= ctr_buf_append_u16_le(                              \
						buf, g_app_data.current[i]->last_sample * 100.f);  \
				}                                                                  \
			}                                                                          \
		}                                                                                  \
	} while (0)

/* TODO - encapsulate to own object? collision with mpt112 therm_temperature*/
#define CTR_ENCODE_LRW_S1(buf)                                                                     \
	do {                                                                                       \
		struct app_data_iaq *iaqdata = &g_app_data.iaq;                                    \
		if (isnan(iaqdata->sensors.last_temperature)) {                                    \
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));                           \
		} else {                                                                           \
			int16_t val = iaqdata->sensors.last_temperature * 100.f;                   \
			ret |= ctr_buf_append_s16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(iaqdata->sensors.last_humidity)) {                                       \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint16_t val = iaqdata->sensors.last_humidity * 100.f;                     \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(iaqdata->sensors.last_altitude)) {                                       \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint16_t val = iaqdata->sensors.last_altitude * 100.f;                     \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(iaqdata->sensors.last_co2_conc)) {                                       \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint16_t val = iaqdata->sensors.last_co2_conc;                             \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(iaqdata->sensors.last_illuminance)) {                                    \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint16_t val = iaqdata->sensors.last_illuminance * 100.f;                  \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
		if (isnan(iaqdata->sensors.last_pressure)) {                                       \
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));                           \
		} else {                                                                           \
			uint32_t val = iaqdata->sensors.last_pressure;                             \
			ret |= ctr_buf_append_u16_le(buf, val);                                    \
		}                                                                                  \
	} while (0)

#endif /* CHESTER_INCLUDE_CTR_ENCODE_LRW_H_ */
