#ifndef APP_MEASURE_H_
#define APP_MEASURE_H_

/* CHESTER includes */
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_timer g_app_measure_timer;

int app_measure(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MEASURE_H_ */
