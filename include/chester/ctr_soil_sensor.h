/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_SOIL_SENSOR_H_
#define CHESTER_INCLUDE_CTR_SOIL_SENSOR_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_soil_sensor_scan(void);
int ctr_soil_sensor_get_count(void);
int ctr_soil_sensor_read(int index, uint64_t *serial_number, float *temperature, int *moisture);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_SOIL_SENSOR_H_ */
