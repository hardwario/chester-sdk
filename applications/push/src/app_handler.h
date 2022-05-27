#ifndef APP_HANDLER_H_
#define APP_HANDLER_H_

/* CHESTER includes */
#include <ctr_lrw.h>
#include <drivers/ctr_z.h>

/* Zephyr includes */
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param);
void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_HANDLER_H_ */
