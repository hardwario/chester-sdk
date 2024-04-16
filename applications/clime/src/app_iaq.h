/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_IAQ_H_
#define APP_IAQ_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_sem g_app_init_sem;

int app_iaq_led_task(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_IAQ_H_ */
