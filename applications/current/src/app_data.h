#ifndef APP_DATA_H_
#define APP_DATA_H_

#include "app_config.h"

/* CHESTER includes */
#include <chester/ctr_lte.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

/* TODO Delete */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_data_channel_measurement {
	int64_t timestamp;
	int avg[APP_CONFIG_CHANNEL_COUNT];
	int rms[APP_CONFIG_CHANNEL_COUNT];
};

struct app_data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float acceleration_x;
	float acceleration_y;
	float acceleration_z;
	int orientation;
	int channel_measurement_count;
	struct app_data_channel_measurement channel_measurements[128];
};

extern struct app_data g_app_data;

/* TODO Delete */
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
