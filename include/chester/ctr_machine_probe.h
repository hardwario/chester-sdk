/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_MACHINE_PROBE_H_
#define CHESTER_INCLUDE_CTR_MACHINE_PROBE_H_

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_machine_probe_scan(void);
int ctr_machine_probe_get_count(void);
int ctr_machine_probe_read_thermometer(int index, uint64_t *serial_number, float *temperature);
int ctr_machine_probe_read_hygrometer(int index, uint64_t *serial_number, float *temperature,
				      float *humidity);
int ctr_machine_probe_read_lux_meter(int index, uint64_t *serial_number, float *illuminance);
int ctr_machine_probe_read_magnetometer(int index, uint64_t *serial_number, float *magnetic_field);
int ctr_machine_probe_read_accelerometer(int index, uint64_t *serial_number, float *accel_x,
					 float *accel_y, float *accel_z, int *orientation);
int ctr_machine_probe_enable_tilt_alert(int index, uint64_t *serial_number, int threshold,
					int duration);
int ctr_machine_probe_disable_tilt_alert(int index, uint64_t *serial_number);
int ctr_machine_probe_get_tilt_alert(int index, uint64_t *serial_number, bool *is_active);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_MACHINE_PROBE_H_ */
