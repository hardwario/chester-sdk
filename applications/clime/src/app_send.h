/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_SEND_H_
#define APP_SEND_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_send(void);

#if defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)
int app_send_lrw_x4_line_alert(bool line_connected_event);
#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B) */

#ifdef __cplusplus
}
#endif

#endif /* APP_SEND_H_ */
