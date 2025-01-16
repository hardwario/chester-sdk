/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#ifdef __cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

void app_sensor_trigger_clear(void);
int app_sensor_counter_aggreg(void);
void app_sensor_counter_clear(void);
int app_sensor_voltage_sample(void);
int app_sensor_voltage_aggreg(void);
void app_sensor_voltage_clear(void);
int app_sensor_current_sample(void);
int app_sensor_current_aggreg(void);
void app_sensor_current_clear(void);

int app_sensor_hygro_sample(void);
int app_sensor_hygro_aggreg(void);
void app_sensor_hygro_clear(void);

int app_sensor_w1_therm_sample(void);
int app_sensor_w1_therm_aggreg(void);
int app_sensor_w1_therm_clear(void);

int app_sensor_soil_sensor_sample(void);
int app_sensor_soil_sensor_aggreg(void);
int app_sensor_soil_sensor_clear(void);

int app_sensor_ble_tag_sample(void);
int app_sensor_ble_tag_aggreg(void);
int app_sensor_ble_tag_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_H_ */
