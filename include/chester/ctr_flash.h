/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_CTR_FLASH_H_
#define CHESTER_INCLUDE_CTR_FLASH_H_

#ifdef __cplusplus
extern "C" {
#endif

int ctr_flash_acquire(void);
int ctr_flash_release(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_FLASH_H_ */
