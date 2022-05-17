#ifndef APP_DATA_H_
#define APP_DATA_H_

/* CHESTER includes */
#include <ctr_lte.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

/* TODO Delete */
#include <zephyr.h>

#define W1_THERM_COUNT 10
#define W1_THERM_MAX_SAMPLES 128

#ifdef __cplusplus
extern "C" {
#endif

struct w1_therm {
	uint64_t serial_number;
	uint64_t timestamp;
	int sample_count;
	float samples[W1_THERM_MAX_SAMPLES];
};

struct data {
	float batt_voltage_rest;
	float batt_voltage_load;
	float batt_current_load;
	float therm_temperature;
	float accel_x;
	float accel_y;
	float accel_z;
	int accel_orientation;
	struct w1_therm w1_therm[W1_THERM_COUNT];
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
