/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_X0_MAP_H_
#define APP_X0_MAP_H_

#include "app_data.h" /* APP_DATA_NUM_CHANNELS */
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/drivers/ctr_x0.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

/* 0-based channel index (0..APP_DATA_NUM_CHANNELS-1) -> X0 channel / ADC
 * channel. Single definition (app_sensor_drv.c); replaces the copies that were
 * duplicated across app_init.c / app_shell.c / app_sensor.c / app_sensor_drv.c. */
extern const enum ctr_x0_channel app_x0_channel_map[APP_DATA_NUM_CHANNELS];
extern const enum ctr_adc_channel app_x0_adc_map[APP_DATA_NUM_CHANNELS];

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#ifdef __cplusplus
}
#endif

#endif /* APP_X0_MAP_H_ */
