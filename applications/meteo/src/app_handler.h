/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_edge.h>
#include <chester/ctr_lte.h>
#include <chester/drivers/ctr_s1.h>
#include <chester/drivers/ctr_z.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FEATURE_SUBSYSTEM_LTE)
void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param);
#endif /* defined(FEATURE_SUBSYSTEM_LTE) */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
void app_handler_edge_wind_speed_callback(struct ctr_edge *edge, enum ctr_edge_event edge_event,
					  void *user_data);
void app_handler_edge_rainfall_callback(struct ctr_edge *edge, enum ctr_edge_event edge_event,
					void *user_data);
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B) */

#if defined(FEATURE_HARDWARE_CHESTER_Z)
void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *param);
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_SUBSYSTEM_BUTTON)
void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data);
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
