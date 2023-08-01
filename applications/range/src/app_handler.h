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
#include <chester/drivers/ctr_s1.h>
#include <chester/drivers/ctr_z.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_SHIELD_CTR_LTE)
void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param);
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

#if defined(CONFIG_SHIELD_CTR_Z)
void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *param);
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_CTR_BUTTON)
void app_handler_ctr_button(enum ctr_button_channel chan, enum ctr_button_event ev, int val,
			    void *user_data);
#endif /* defined(CONFIG_CTR_BUTTON) */

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
