/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_HYGRO_H_
#define CHESTER_INCLUDE_CTR_HYGRO_H_

#ifdef __cplusplus
extern "C" {
#endif

int ctr_hygro_read(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_HYGRO_H_ */
