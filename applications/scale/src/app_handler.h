/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/drivers/ctr_z.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FEATURE_HARDWARE_CHESTER_Z)
void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *param);
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BUTTON)
void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data);
#endif /* defined(CONFIG_SHIELD_BUTTON) */

#if defined(FEATURE_SUBSYSTEM_LRW)
void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param);
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
