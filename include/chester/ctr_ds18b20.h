/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_DS18B20_H_
#define CHESTER_INCLUDE_CTR_DS18B20_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_ds18b20_scan(void);
int ctr_ds18b20_get_count(void);
int ctr_ds18b20_read(int index, uint64_t *serial_number, float *temperature);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_DS18B20_H_ */
