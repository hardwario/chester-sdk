/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_LOOP_H_
#define APP_LOOP_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_sem g_app_loop_sem;
extern atomic_t g_app_loop_measure_weight;
extern atomic_t g_app_loop_measure_people;
extern atomic_t g_app_loop_send;

int app_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_LOOP_H_ */
