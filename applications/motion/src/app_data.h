#ifndef APP_DATA_H_
#define APP_DATA_H_

#include <stdbool.h>
#include <stdint.h>
#include "feature.h"

#include <zephyr/kernel.h>

#define APP_DATA_MAX_MEASUREMENTS  30


#ifdef _cplusplus
extern "C" {
#endif




enum app_data_direction {
	APP_DATA_DIRECTION_NONE = 0,
	APP_DATA_DIRECTION_LEFT_TO_RIGHT,
	APP_DATA_DIRECTION_RIGHT_TO_LEFT,
};


struct app_data_measurement_value {
	int detect_left;
	int detect_right;
	int motion_left;
	int motion_right;
};


struct app_data_measurement {
	uint64_t timestamp;
	struct app_data_measurement_value value;
};

struct app_data {
	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

#if defined(CONFIG_CTR_S3)
	int motion_count_l;
	int motion_count_r;
	int motion_count_left;
	int motion_count_right;

	int total_detect_left;
	int total_detect_right;
	int total_motion_left;
	int total_motion_right;

	struct app_data_measurement motion_samples[APP_DATA_MAX_MEASUREMENTS];
	int motion_sample_count;
	enum app_data_direction last_motion_direction;
#endif /* defined(CONFIG_CTR_S3) */
	};

extern struct app_data g_app_data;

extern struct k_mutex g_app_data_lte_eval_mut;
extern bool g_app_data_lte_eval_valid;
extern struct ctr_lte_eval g_app_data_lte_eval;

void app_data_lock(void);
void app_data_unlock(void);

#ifdef _cplusplus
}
#endif

#endif /* APP_DATA_H_ */
