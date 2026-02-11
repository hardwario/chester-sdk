/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_WORK_H_
#define APP_WORK_H_

#ifdef _cplusplus
extern "C" {
#endif

typedef void (*app_work_sample_cb_t)(void *user_data);

int app_work_init(void);
void app_work_sample(void);
void app_work_sample_with_cb(app_work_sample_cb_t cb, void *user_data);
void app_work_send(void);

#ifdef _cplusplus
}
#endif

#endif /* APP_WORK_H */
