/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

#include "feature.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
int app_sensor_ble_tag_sample(void);
int app_sensor_ble_tag_aggreg(void);
int app_sensor_ble_tag_clear(void);
#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_H_ */