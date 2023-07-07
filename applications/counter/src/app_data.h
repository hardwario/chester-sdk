/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

/* CHESTER includes */
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
	
#define APP_DATA_MAX_BACKUP_EVENTS 32
#define APP_DATA_MAX_TAMPER_EVENTS 32

#if defined(CONFIG_APP_TAMPER)
struct app_data_tamper_event {
	int64_t timestamp;
	bool activated;
};

struct app_data_tamper {
	bool activated;
	int event_count;
	struct app_data_tamper_event events[APP_DATA_MAX_TAMPER_EVENTS];
};
#endif /* defined(CONFIG_APP_TAMPER) */

#if defined(CONFIG_SHIELD_CTR_Z)
struct app_data_backup_event {
	int64_t timestamp;
	bool connected;
};

struct app_data_backup {
	float line_voltage;
	float battery_voltage;
	bool line_present;
	int event_count;
	struct app_data_backup_event events[APP_DATA_MAX_BACKUP_EVENTS];
};
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

struct app_data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float accel_x;
	float accel_y;
	float accel_z;
	int accel_orientation;

#if defined(CONFIG_SHIELD_CTR_X0_A)
	uint64_t counter_ch1_total;
	uint64_t counter_ch1_delta;
	uint64_t counter_ch2_total;
	uint64_t counter_ch2_delta;
	uint64_t counter_ch3_total;
	uint64_t counter_ch3_delta;
	uint64_t counter_ch4_total;
	uint64_t counter_ch4_delta;
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_X0_B)
	uint64_t counter_ch5_total;
	uint64_t counter_ch5_delta;
	uint64_t counter_ch6_total;
	uint64_t counter_ch6_delta;
	uint64_t counter_ch7_total;
	uint64_t counter_ch7_delta;
	uint64_t counter_ch8_total;
	uint64_t counter_ch8_delta;
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

#if defined(CONFIG_APP_TAMPER)
	struct app_data_tamper tamper;
#endif /* defined(CONFIG_APP_TAMPER) */

#if defined(CONFIG_SHIELD_CTR_Z)
	struct app_data_backup backup;
#endif /* defined(CONFIG_SHIELD_CTR_Z) */
};

extern struct app_data g_app_data;
extern struct k_mutex g_app_data_lock;

static inline void app_data_lock(void)
{
	k_mutex_lock(&g_app_data_lock, K_FOREVER);
}

static inline void app_data_unlock(void)
{
	k_mutex_unlock(&g_app_data_lock);
}

/* TODO Delete */
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
