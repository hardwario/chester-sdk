/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_timer g_app_measure_timer;

int app_sensor_sample(void);
int app_sensor_radon_sample(void);
int app_sensor_radon_aggreg(void);
int app_sensor_radon_clear(void);
int app_sensor_ble_tag_sample(void);
int app_sensor_ble_tag_aggreg(void);
int app_sensor_ble_tag_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_H_ */
