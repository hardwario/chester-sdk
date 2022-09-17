#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/drivers/ctr_s1.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_SHIELD_CTR_LRW)
void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param);
#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

#if defined(CONFIG_SHIELD_CTR_LTE)
void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param);
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

#if defined(CONFIG_SHIELD_CTR_S1)
void ctr_s1_event_handler(const struct device *dev, enum ctr_s1_event event, void *user_data);
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
