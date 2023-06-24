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

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DATA_MAX_BACKUP_EVENTS 32
#define APP_DATA_MAX_BUTTON_EVENTS 8

#define APP_DATA_BUTTON_COUNT 5

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

enum app_data_button_event_type {
	APP_DATA_BUTTON_EVENT_X_CLICK = 0,
	APP_DATA_BUTTON_EVENT_X_HOLD = 1,
	APP_DATA_BUTTON_EVENT_1_CLICK = 2,
	APP_DATA_BUTTON_EVENT_1_HOLD = 3,
	APP_DATA_BUTTON_EVENT_2_CLICK = 4,
	APP_DATA_BUTTON_EVENT_2_HOLD = 5,
	APP_DATA_BUTTON_EVENT_3_CLICK = 6,
	APP_DATA_BUTTON_EVENT_3_HOLD = 7,
	APP_DATA_BUTTON_EVENT_4_CLICK = 8,
	APP_DATA_BUTTON_EVENT_4_HOLD = 9,
};

struct app_data_button_event {
	int64_t timestamp;
	enum app_data_button_event_type type;
};

struct app_data_button {
	atomic_t click_count;
	atomic_t hold_count;
	int event_count;
	struct app_data_button_event events[APP_DATA_MAX_BUTTON_EVENTS];
};

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

struct app_data {

	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

#if defined(CONFIG_SHIELD_CTR_Z)
	struct app_data_backup backup;
	struct app_data_button button[APP_DATA_BUTTON_COUNT];
#endif /* defined(CONFIG_SHIELD_CTR_Z) */
};

extern struct app_data g_app_data;

/* TODO Delete */
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

void app_data_lock(void);
void app_data_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
