/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

/* CHESTER includes */
#include <chester/ctr_lte.h>
#include <chester/drivers/people_counter.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

/* TODO Delete */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct weight_measurement {
	int timestamp_offset;
	int32_t a1_raw;
	int32_t a2_raw;
	int32_t b1_raw;
	int32_t b2_raw;
};

#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
struct people_measurement {
	int timestamp_offset;
	struct people_counter_measurement measurement;
	bool is_valid;
};
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */

struct data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float acceleration_x;
	float acceleration_y;
	float acceleration_z;
	int orientation;
	int weight_measurement_count;
	int64_t weight_measurement_timestamp;
	struct weight_measurement weight_measurements[128];
#if defined(CONFIG_SHIELD_PEOPLE_COUNTER)
	int people_measurement_count;
	int64_t people_measurement_timestamp;
	struct people_measurement people_measurements[128];
#endif /* defined(CONFIG_SHIELD_PEOPLE_COUNTER) */
};

extern struct data g_app_data;

/* TODO Delete */
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
