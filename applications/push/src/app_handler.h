#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#if defined(CONFIG_SHIELD_CTR_LRW)
#include <ctr_lrw.h>
#endif

#if defined(CONFIG_SHIELD_CTR_LTE)
#include <ctr_lte.h>
#endif

#include <drivers/ctr_z.h>

/* Zephyr includes */
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_SHIELD_CTR_LRW)
void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param);
#endif

#if defined(CONFIG_SHIELD_CTR_LTE)
void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param);
#endif

void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
