/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_BACKUP_H_
#define APP_BACKUP_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_backup_init(void);
int app_backup_sample(void);
int app_backup_clear(void);

#if defined(FEATURE_HARDWARE_CHESTER_Z)
bool app_backup_z_is_available(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* APP_BACKUP_H_ */
