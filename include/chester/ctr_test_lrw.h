/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_TEST_LRW_H_
#define CHESTER_INCLUDE_CTR_TEST_LRW_H_

#define CTR_TEST_LRW_JSON_BATT(data)                                                               \
	cJSON_AddNumberToObject(data, "voltage_rest", g_app_data.system_voltage_rest / 1000.f);    \
	cJSON_AddNumberToObject(data, "voltage_load", g_app_data.system_voltage_load / 1000.f);    \
	cJSON_AddNumberToObject(data, "current_load", g_app_data.system_current_load);

#define CTR_TEST_LRW_JSON_HYGRO(data)                                                              \
	cJSON_AddNumberToObject(data, "hygro_temperature",                                         \
				g_app_data.hygro.last_sample_temperature);                         \
	cJSON_AddNumberToObject(data, "hygro_humidity", g_app_data.hygro.last_sample_humidity);

#define CTR_TEST_LRW_JSON_THERM(data)                                                              \
	cJSON_AddNumberToObject(data, "therm_temperature", g_app_data.therm_temperature);

#define CTR_TEST_LRW_JSON_ORIENTATION(data)                                                        \
	if (g_app_data.accel_orientation == 0xff) {                                                \
		cJSON_AddNullToObject(data, "orientation");                                        \
	} else {                                                                                   \
		cJSON_AddNumberToObject(data, "orientation", g_app_data.accel_orientation);        \
	}

#define CTR_TEST_LRW_JSON_W1_THERMOMETERS(data)                                                    \
	do {                                                                                       \
		cJSON *w1_therms = cJSON_CreateArray();                                            \
		for (int i = 0; i < APP_DATA_W1_THERM_COUNT; ++i) {                                \
			if (g_app_data.w1_therm.sensor[i].serial_number == 0) {                    \
				continue;                                                          \
			}                                                                          \
			cJSON_AddItemToArray(                                                      \
				w1_therms,                                                         \
				cJSON_CreateNumber(                                                \
					g_app_data.w1_therm.sensor[i].last_sample_temperature));   \
		}                                                                                  \
		cJSON_AddItemToObject(data, "w1_thermometers", w1_therms);                         \
	} while (0)

#define CTR_TEST_LRW_JSON_BLE_TAGS(data)                                                           \
	do {                                                                                       \
		cJSON *ble_tags = cJSON_CreateArray();                                             \
		for (int i = 0; i < CTR_BLE_TAG_COUNT; ++i) {                                      \
			if (g_app_data.ble_tag.sensor[i].addr[0] == 0x00) {                        \
				continue;                                                          \
			}                                                                          \
			cJSON *ble = cJSON_CreateObject();                                         \
			cJSON_AddNumberToObject(                                                   \
				ble, "temperature",                                                \
				g_app_data.ble_tag.sensor[i].last_sample_temperature);             \
			cJSON_AddNumberToObject(                                                   \
				ble, "humidity",                                                   \
				g_app_data.ble_tag.sensor[i].last_sample_humidity);                \
			cJSON_AddItemToArray(ble_tags, ble);                                       \
		}                                                                                  \
		cJSON_AddItemToObject(data, "ble_tags", ble_tags);                                 \
	} while (0)

#define CTR_TEST_LRW_JSON_X0_A_INPUTS(data)                                                        \
	do {                                                                                       \
		cJSON *inputs_a = cJSON_CreateArray();                                             \
		for (int i = 0; i < APP_DATA_NUM_CHANNELS; ++i) {                                  \
			if (g_app_data.trigger[i]) {                                               \
				cJSON *trigger = cJSON_CreateObject();                             \
				cJSON_AddStringToObject(trigger, "type", "trigger");               \
				cJSON_AddBoolToObject(trigger, "state",                            \
						      g_app_data.trigger[i]->events[0].is_active); \
				cJSON_AddBoolToObject(trigger, "trigger_active",                   \
						      g_app_data.trigger[i]->trigger_active);      \
				cJSON_AddBoolToObject(trigger, "trigger_inactive",                 \
						      !g_app_data.trigger[i]->trigger_active);     \
				cJSON_AddItemToArray(inputs_a, trigger);                           \
			}                                                                          \
			if (g_app_data.counter[i]) {                                               \
				cJSON *counter = cJSON_CreateObject();                             \
				cJSON_AddStringToObject(counter, "type", "counter");               \
				cJSON_AddNumberToObject(counter, "count",                          \
							g_app_data.counter[i]->value);             \
				cJSON_AddNumberToObject(counter, "delta",                          \
							g_app_data.counter[i]->delta);             \
				cJSON_AddItemToArray(inputs_a, counter);                           \
			}                                                                          \
			if (g_app_data.voltage[i]) {                                               \
				cJSON *voltage = cJSON_CreateObject();                             \
				cJSON_AddStringToObject(voltage, "type", "voltage");               \
				cJSON_AddNumberToObject(voltage, "voltage",                        \
							g_app_data.voltage[i]->last_sample);       \
				cJSON_AddItemToArray(inputs_a, voltage);                           \
			}                                                                          \
			if (g_app_data.current[i]) {                                               \
				cJSON *current = cJSON_CreateObject();                             \
				cJSON_AddStringToObject(current, "type", "current");               \
				cJSON_AddNumberToObject(current, "current",                        \
							g_app_data.current[i]->last_sample);       \
				cJSON_AddItemToArray(inputs_a, current);                           \
			}                                                                          \
		}                                                                                  \
		cJSON_AddItemToObject(data, "inputs_a", inputs_a);                                 \
	} while (0)

#define CTR_TEST_LRW_JSON_SOIL_SENSOR(data)                                                        \
	do {                                                                                       \
		cJSON *soil_sensor = cJSON_CreateArray();                                          \
		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; ++i) {                             \
			if (g_app_data.soil_sensor.sensor[i].serial_number == 0x00) {              \
				continue;                                                          \
			}                                                                          \
			cJSON *soil = cJSON_CreateObject();                                        \
			cJSON_AddNumberToObject(                                                   \
				soil, "temperature",                                               \
				g_app_data.soil_sensor.sensor[i].last_temperature);                \
			cJSON_AddNumberToObject(soil, "moisture",                                  \
						g_app_data.soil_sensor.sensor[i].last_moisture);   \
			cJSON_AddItemToArray(soil_sensor, soil);                                   \
		}                                                                                  \
		cJSON_AddItemToObject(data, "soil_sensors", soil_sensor);                          \
	} while (0)

#define CTR_TEST_LRW_JSON_RTD_THERMOMETERS(data)                                                   \
	do {                                                                                       \
		cJSON *rtd_therms = cJSON_CreateArray();                                           \
		for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; ++i) {                               \
			cJSON_AddItemToArray(                                                      \
				rtd_therms,                                                        \
				cJSON_CreateNumber(                                                \
					g_app_data.rtd_therm.sensor[i].last_sample_temperature));  \
		}                                                                                  \
		cJSON_AddItemToObject(data, "rtd_thermometers", rtd_therms);                       \
	} while (0)

#define CTR_TEST_LRW_JSON_TC_THERMOMETERS(data)                                                    \
	do {                                                                                       \
		cJSON *tc_therms = cJSON_CreateArray();                                            \
		for (int i = 0; i < APP_DATA_TC_THERM_COUNT; ++i) {                                \
			cJSON_AddItemToArray(                                                      \
				tc_therms,                                                         \
				cJSON_CreateNumber(                                                \
					g_app_data.tc_therm.sensor[i].last_sample_temperature));   \
		}                                                                                  \
		cJSON_AddItemToObject(data, "tc_thermometers", tc_therms);                         \
	} while (0)

#define CTR_TEST_LRW_JSON_IAQ(data)                                                                \
	do {                                                                                       \
		struct app_data_iaq_sensors *sensors = &g_app_data.iaq.sensors;                    \
		if (sensors->last_temperature == 0x7fff) {                                         \
			cJSON_AddNullToObject(data, "therm_temperature");                          \
		} else {                                                                           \
			cJSON_AddNumberToObject(data, "therm_temperature",                         \
						(double)sensors->last_temperature);                \
		}                                                                                  \
		if (sensors->last_humidity == 0xffff) {                                            \
			cJSON_AddNullToObject(data, "hygro_humidity");                             \
		} else {                                                                           \
			cJSON_AddNumberToObject(data, "hygro_humidity", sensors->last_humidity);   \
		}                                                                                  \
		if (sensors->last_altitude == 0xffff) {                                            \
			cJSON_AddNullToObject(data, "altitude");                                   \
		} else {                                                                           \
			cJSON_AddNumberToObject(data, "altitude", sensors->last_altitude);         \
		}                                                                                  \
		if (sensors->last_co2_conc == 0xffff) {                                            \
			cJSON_AddNullToObject(data, "co2");                                        \
		} else {                                                                           \
			cJSON_AddNumberToObject(data, "co2", sensors->last_co2_conc);              \
		}                                                                                  \
		if (sensors->last_illuminance == 0xffff) {                                         \
			cJSON_AddNullToObject(data, "illuminance");                                \
		} else {                                                                           \
			cJSON_AddNumberToObject(data, "illuminance", sensors->last_illuminance);   \
		}                                                                                  \
		if (sensors->last_pressure == 0xffff) {                                            \
			cJSON_AddNullToObject(data, "pressure");                                   \
		} else {                                                                           \
			cJSON_AddNumberToObject(data, "pressure",                                  \
						(double)(sensors->last_pressure));                 \
		}                                                                                  \
	} while (0)

#define CTR_TEST_LRW_JSON_METEO(data)                                                              \
	cJSON_AddNumberToObject(data, "wind_speed", g_app_data.meteo.wind_speed.last_sample);      \
	cJSON_AddNumberToObject(data, "wind_direction",                                            \
				g_app_data.meteo.wind_direction.last_sample);                      \
	cJSON_AddNumberToObject(data, "rainfall", g_app_data.meteo.rainfall.last_sample);

#define CTR_TEST_LRW_JSON_BAROMETER_TAG(data)                                                      \
	cJSON_AddNumberToObject(data, "barometer",                                                 \
				g_app_data.barometer.pressure.last_sample / 1000.f);

#endif /* CHESTER_INCLUDE_CTR_TEST_LRW_H_ */
