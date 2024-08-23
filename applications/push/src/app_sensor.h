/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MEASURE_H_
#define APP_MEASURE_H_

#ifdef __cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

int app_sensor_ble_tag_sample(void);
int app_sensor_ble_tag_aggreg(void);
int app_sensor_ble_tag_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MEASURE_H_ */
