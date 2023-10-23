/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_WEIGHT_PROBE_H_
#define CHESTER_INCLUDE_CTR_WEIGHT_PROBE_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_weight_probe_scan(void);
int ctr_weight_probe_get_count(void);
int ctr_weight_probe_read(int index, uint64_t *serial_number, int32_t *result);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_WEIGHT_PROBE_H_ */
