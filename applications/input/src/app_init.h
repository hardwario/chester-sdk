#ifndef APP_INIT_H_
#define APP_INIT_H_

/* Zephyr includes */
#include <zephyr/zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct k_sem g_app_init_sem;

int app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_INIT_H_ */
