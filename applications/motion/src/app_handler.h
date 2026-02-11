/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <chester/ctr_button.h>
#include <chester/ctr_lte.h>
#include <chester/drivers/ctr_s3.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FEATURE_SUBSYSTEM_LTE)
void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param);
#endif /* defined(FEATURE_SUBSYSTEM_LTE) */

#if defined(FEATURE_SUBSYSTEM_BUTTON)
void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data);
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

#if defined(CONFIG_CTR_S3)
void app_handler_ctr_s3(const struct device *dev, enum ctr_s3_event event, void *user_data);

struct app_detection_event {
	int direction;  /* 1 = L->R, 2 = R->L */
	int delta_ms;
	int detect_left;
	int detect_right;
	int motion_left;
	int motion_right;
};

typedef void (*app_motion_detection_cb_t)(const struct app_detection_event *event, void *user_data);

void app_handler_set_detection_cb(app_motion_detection_cb_t cb, void *user_data);
void app_handler_clear_detection_cb(void);
#endif /* defined(CONFIG_CTR_S3) */

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
