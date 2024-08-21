/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_BACKUP_H_
#define APP_BACKUP_H_

#ifdef __cplusplus
extern "C" {
#endif

int app_backup_init(void);
int app_backup_sample(void);
int app_backup_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BACKUP_H_ */
