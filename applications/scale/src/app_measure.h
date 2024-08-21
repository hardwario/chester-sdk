/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_MEASURE_H_
#define APP_MEASURE_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_measure_weight(void);
int app_measure_people(void);
int app_measure_clear(void);
int app_test_measure(const struct shell *shell);
#ifdef __cplusplus
}
#endif

#endif /* APP_MEASURE_H_ */
