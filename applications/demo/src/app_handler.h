/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data);
void app_handler_cloud_event(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
			     void *param);

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
