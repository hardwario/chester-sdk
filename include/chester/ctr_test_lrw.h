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
	cJSON_AddNumberToObject(data, "therm", g_app_data.therm_temperature);

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

#endif /* CHESTER_INCLUDE_CTR_TEST_LRW_H_ */
