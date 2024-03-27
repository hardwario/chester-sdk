/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_INIT_H_
#define APP_INIT_H_

/* CHESTER includes */
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct ctr_wdog_channel g_app_wdog_channel;

int app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INIT_H_ */
