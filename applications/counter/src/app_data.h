#ifndef APP_DATA_H_
#define APP_DATA_H_

/* CHESTER includes */
#include <ctr_lte.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

/* Zephyr includes */
#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

struct data {
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
};

extern struct data g_app_data;
extern struct k_mutex g_app_data_lock;

/* TODO Delete */
extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
