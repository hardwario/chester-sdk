#ifndef APP_DATA_H_
#define APP_DATA_H_

/* Standard includes */
#include <stdbool.h>

/* CHESTER includes */
#if defined(CONFIG_SHIELD_CTR_LTE)
#include <chester/ctr_lte.h>
#endif

/* Zephyr includes */
#include <zephyr/zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_data_errors {
	bool orientation;
	bool acceleration_x;
	bool acceleration_y;
	bool acceleration_z;
	bool int_temperature;
	bool ext_temperature;
	bool ext_humidity;
	bool line_present;
	bool line_voltage;
	bool bckp_voltage;
	bool batt_voltage_rest;
	bool batt_voltage_load;
	bool batt_current_load;
};

struct app_data_states {
	int orientation;
	float acceleration_x;
	float acceleration_y;
	float acceleration_z;
	float int_temperature;
	float ext_temperature;
	float ext_humidity;
	bool line_present;
	float line_voltage;
	float bckp_voltage;
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
};

struct app_data_events {
	atomic_t device_tilt;
	atomic_t button_x_click;
	atomic_t button_x_hold;
	atomic_t button_1_click;
	atomic_t button_1_hold;
	atomic_t button_2_click;
	atomic_t button_2_hold;
	atomic_t button_3_click;
	atomic_t button_3_hold;
	atomic_t button_4_click;
	atomic_t button_4_hold;
};

struct app_data_sources {
	atomic_t device_boot;
	atomic_t button_x_click;
	atomic_t button_x_hold;
	atomic_t button_1_click;
	atomic_t button_1_hold;
	atomic_t button_2_click;
	atomic_t button_2_hold;
	atomic_t button_3_click;
	atomic_t button_3_hold;
	atomic_t button_4_click;
	atomic_t button_4_hold;
};

struct app_data {
	struct app_data_errors errors;
	struct app_data_states states;
	struct app_data_events events;
	struct app_data_sources sources;
};

extern struct app_data g_app_data;

/* TODO Delete */
#if defined(CONFIG_SHIELD_CTR_LTE)
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
