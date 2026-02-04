/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_DATA_H_
#define APP_DATA_H_

#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_ble_tag.h>
#include <chester/application/ctr_data.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

/* TODO Delete */
#include <zephyr/kernel.h>

#define APP_DATA_MAX_MEASUREMENTS  32
#define APP_DATA_MAX_SAMPLES       32
#define APP_DATA_MAX_BACKUP_EVENTS 32
#define APP_DATA_MAX_HYGRO_EVENTS  32

#define APP_DATA_W1_THERM_COUNT    10
#define APP_DATA_SOIL_SENSOR_COUNT 10

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
#define APP_DATA_MAX_BLE_TAG_SAMPLES      16
#define APP_DATA_MAX_BLE_TAG_MEASUREMENTS 16
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FEATURE_HARDWARE_CHESTER_Z)
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
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_M)
struct app_data_sensecap {
	int sample_count;
	float air_temperature_samples[APP_DATA_MAX_SAMPLES];
	float air_humidity_samples[APP_DATA_MAX_SAMPLES];
	float barometric_pressure_samples[APP_DATA_MAX_SAMPLES];
	float light_intensity_samples[APP_DATA_MAX_SAMPLES];
	float wind_direction_avg_samples[APP_DATA_MAX_SAMPLES];
	float wind_speed_avg_samples[APP_DATA_MAX_SAMPLES];
	float accumulated_rainfall_samples[APP_DATA_MAX_SAMPLES];
	float pm2_5_samples[APP_DATA_MAX_SAMPLES];
	float pm10_samples[APP_DATA_MAX_SAMPLES];
	float co2_samples[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct ctr_data_aggreg air_temperature_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg air_humidity_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg barometric_pressure_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg light_intensity_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg wind_direction_avg_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg wind_speed_avg_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg accumulated_rainfall_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg pm2_5_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg pm10_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg co2_measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

struct app_data_cubic_pm {
	int sample_count;
	float version_samples[APP_DATA_MAX_SAMPLES];
	float tsp_samples[APP_DATA_MAX_SAMPLES];
	float pm1_0_samples[APP_DATA_MAX_SAMPLES];
	float pm2_5_samples[APP_DATA_MAX_SAMPLES];
	float pm10_samples[APP_DATA_MAX_SAMPLES];
	float gas_flow_samples[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct ctr_data_aggreg version_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg tsp_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg pm1_0_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg pm2_5_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg pm10_measurements[APP_DATA_MAX_MEASUREMENTS];
	struct ctr_data_aggreg gas_flow_measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

struct app_data_meteo_raw {
	long long int air_temperature;
	int air_humidity;
	int barometric_pressure;
	float light_intensity;
	float minimum_wind_direction;
	float maximum_wind_direction;
	float average_wind_direction;
	float minimum_wind_speed;
	float maximum_wind_speed;
	float average_wind_speed;
	float accumulated_rainfall;
	float accumulated_rainfall_duration;
	float rain_intensity;
	float maximum_rainfall;
	float heating_temperature;
	int dumping_state;
	float pm25;
	float pm10;
	float co2;
	float noise_intensity;
	float global_solar_radiation;
	float sunshine_duration;
	float lambrecht_wind_speed;
	float lambrecht_wind_speed_avg;
	float lambrecht_wind_direction;
	float lambrecht_rainfall_total;
	float lambrecht_rainfall_intensity;
	float lambrecht_illuminance;
};
#endif /* defined(FEATURE_HARDWARE_CHESTER_X2_MODBUS_A ) ||                                        \
	  defined(FEATURE_HARDWARE_CHESTER_METEO_M) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)

struct app_data_meteo_wind_speed {
	int64_t last_sample_timestamp;
	int sample_count;
	float samples[APP_DATA_MAX_SAMPLES];
	float last_sample;

	int measurement_count;
	struct ctr_data_aggreg measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

struct app_data_meteo_wind_direction {
	int sample_count;
	float samples[APP_DATA_MAX_SAMPLES];
	float last_sample;

	int measurement_count;
	float measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

struct app_data_meteo_rainfall {
	int measurement_count;
	float measurements[APP_DATA_MAX_MEASUREMENTS];
	float last_sample;

	int64_t timestamp;
};

struct app_data_meteo {
	struct app_data_meteo_wind_speed wind_speed;
	struct app_data_meteo_wind_direction wind_direction;
	struct app_data_meteo_rainfall rainfall;
};
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
struct app_data_barometer_pressure {
	int sample_count;
	float samples[APP_DATA_MAX_SAMPLES];
	float last_sample;

	int measurement_count;
	struct ctr_data_aggreg measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

struct app_data_barometer {
	struct app_data_barometer_pressure pressure;
};
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)

enum app_data_hygro_event_type {
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_HI_ACTIVATED,
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_HI_DEACTIVATED,
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_LO_ACTIVATED,
	APP_DATA_HYGRO_EVENT_TYPE_ALARM_LO_DEACTIVATED,
};

struct app_data_hygro_event {
	int64_t timestamp;
	enum app_data_hygro_event_type type;
	float value;
};

struct app_data_hygro_measurement {
	struct ctr_data_aggreg temperature;
	struct ctr_data_aggreg humidity;
};

struct app_data_hygro {
	float last_sample_temperature;
	float last_sample_humidity;

	int event_count;
	struct app_data_hygro_event events[APP_DATA_MAX_HYGRO_EVENTS];

	bool alarm_hi_active;
	bool alarm_lo_active;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];
	float samples_humidity[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_hygro_measurement measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)

struct app_data_w1_therm_measurement {
	struct ctr_data_aggreg temperature;
};

struct app_data_w1_therm_sensor {
	uint64_t serial_number;

	float last_sample_temperature;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_w1_therm_measurement measurements[APP_DATA_MAX_MEASUREMENTS];
};

struct app_data_w1_therm {
	struct app_data_w1_therm_sensor sensor[APP_DATA_W1_THERM_COUNT];

	int64_t timestamp;
	atomic_t sample;
	atomic_t aggreg;
};

#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)

struct app_data_soil_sensor_measurement {
	struct ctr_data_aggreg temperature;
	struct ctr_data_aggreg moisture;
};

struct app_data_soil_sensor_sensor {
	uint64_t serial_number;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];
	float samples_moisture[APP_DATA_MAX_SAMPLES];
	float last_temperature;
	float last_moisture;

	int measurement_count;
	struct app_data_soil_sensor_measurement measurements[APP_DATA_MAX_MEASUREMENTS];
};

struct app_data_soil_sensor {
	struct app_data_soil_sensor_sensor sensor[APP_DATA_SOIL_SENSOR_COUNT];
	int64_t timestamp;
};

#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)

struct app_data_ble_tag_measurement {
	struct ctr_data_aggreg temperature;
	struct ctr_data_aggreg humidity;
};

struct app_data_ble_tag_sensor {
	uint8_t addr[BT_ADDR_SIZE];

	int8_t rssi;
	float voltage;

	float last_sample_temperature;
	float last_sample_humidity;

	int sample_count;
	float samples_temperature[APP_DATA_MAX_SAMPLES];
	float samples_humidity[APP_DATA_MAX_SAMPLES];

	int measurement_count;
	struct app_data_ble_tag_measurement measurements[APP_DATA_MAX_BLE_TAG_MEASUREMENTS];
};

struct app_data_ble_tag {
	struct app_data_ble_tag_sensor sensor[CTR_BLE_TAG_COUNT];

	int64_t timestamp;
	atomic_t sample;
	atomic_t aggreg;
};

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)

#define LAMBRECHT_EXPAND_ITEM(sensor, name)                                                        \
	float name##_samples[APP_DATA_MAX_SAMPLES];                                                \
	float name##_measurements[APP_DATA_MAX_MEASUREMENTS];

struct app_data_lambrecht {
	int sample_count;
	int measurement_count;

	float wind_speed_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg wind_speed_measurements[APP_DATA_MAX_MEASUREMENTS];

	float wind_direction_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg wind_direction_measurements[APP_DATA_MAX_MEASUREMENTS];

	float temperature_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg temperature_measurements[APP_DATA_MAX_MEASUREMENTS];

	float humidity_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg humidity_measurements[APP_DATA_MAX_MEASUREMENTS];

	float dew_point_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg dew_point_measurements[APP_DATA_MAX_MEASUREMENTS];

	float pressure_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg pressure_measurements[APP_DATA_MAX_MEASUREMENTS];

	float rainfall_total_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg rainfall_total_measurements[APP_DATA_MAX_MEASUREMENTS];
	float rainfall_total_sum;

	float rainfall_intensity_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg rainfall_intensity_measurements[APP_DATA_MAX_MEASUREMENTS];

	float illuminance_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg illuminance_measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};
#undef LAMBRECHT_EXPAND_ITEM

#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

#if defined(CONFIG_APP_PYRANOMETER)

struct app_data_pyranometer {
	int sample_count;
	int measurement_count;

	float irradiance_1_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg irradiance_1_measurements[APP_DATA_MAX_MEASUREMENTS];

	float irradiance_2_samples[APP_DATA_MAX_SAMPLES];
	struct ctr_data_aggreg irradiance_2_measurements[APP_DATA_MAX_MEASUREMENTS];

	int64_t timestamp;
};

#endif /* defined(CONFIG_APP_PYRANOMETER) */

struct app_data {
	float system_voltage_rest;
	float system_voltage_load;
	float system_current_load;
	float therm_temperature;
	float accel_acceleration_x;
	float accel_acceleration_y;
	float accel_acceleration_z;
	int accel_orientation;

#if defined(FEATURE_HARDWARE_CHESTER_METEO_M)
	struct app_data_sensecap sensecap;
	struct app_data_cubic_pm cubic_pm;
#endif /* defined(FEATURE_HARDWARE_CHESTER_X2_MODBUS_A ) ||                                        \
	  defined(FEATURE_HARDWARE_CHESTER_METEO_M) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	struct app_data_backup backup;
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	struct app_data_meteo meteo;
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	struct app_data_barometer barometer;
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	struct app_data_hygro hygro;
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	struct app_data_w1_therm w1_therm;
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	struct app_data_soil_sensor soil_sensor;
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	struct app_data_ble_tag ble_tag;
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)
	struct app_data_lambrecht lambrecht;
#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

#if defined(CONFIG_APP_PYRANOMETER)
	struct app_data_pyranometer pyranometer;
#endif /* defined(CONFIG_APP_PYRANOMETER) */
};

extern struct app_data g_app_data;

void app_data_add_sample_float(struct ctr_data_aggreg *aggreg_ptr, int sample_count, float sample);

void app_data_lock(void);
void app_data_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_DATA_H_ */
