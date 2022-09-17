#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <chester/ctr_lte.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param);

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
