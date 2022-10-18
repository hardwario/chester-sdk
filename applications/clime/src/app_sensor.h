#ifndef APP_SENSOR_H_
#define APP_SENSOR_H_

/* CHESTER includes */
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

int app_sensor_sample(void);

int app_sensor_iaq_sample(void);
int app_sensor_iaq_aggregate(void);
int app_sensor_iaq_clear(void);

int app_sensor_hygro_sample(void);
int app_sensor_hygro_aggregate(void);
int app_sensor_hygro_clear(void);

int app_sensor_w1_therm_sample(void);
int app_sensor_w1_therm_aggregate(void);
int app_sensor_w1_therm_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSOR_H_ */
