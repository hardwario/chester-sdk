/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#ifdef _cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

int app_sensor_sonar_sample(void);
int app_sensor_sonar_aggreg(void);
int app_sensor_sonar_clear(void);

int app_sensor_hygro_sample(void);
int app_sensor_hygro_aggreg(void);
int app_sensor_hygro_clear(void);

int app_sensor_w1_therm_sample(void);
int app_sensor_w1_therm_aggreg(void);
int app_sensor_w1_therm_clear(void);

#ifdef _cplusplus
}
#endif

#endif /* defined (APP_SENSOR_H_) */
