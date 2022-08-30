#ifndef APP_MEASURE_H_
#define APP_MEASURE_H_

/* Zephyr includes */
#include <zephyr.h>
#include <drivers/ctr_s1.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_timer g_app_measure_timer;

int app_measure(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MEASURE_H_ */
