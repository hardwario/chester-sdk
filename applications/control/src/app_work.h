/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_WORK_H_
#define APP_WORK_H_

#ifdef __cplusplus
extern "C" {
#endif

int app_work_init(void);
void app_work_aggreg(void);
void app_work_sample(void);
void app_work_send(void);
void app_work_poll(void);

#if defined(FEATURE_HARDWARE_CHESTER_Z)
void app_work_backup_update(void);
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#ifdef __cplusplus
}
#endif

#endif /* APP_WORK_H_ */
