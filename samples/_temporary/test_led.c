#include <ctr_bsp.h>
#include <ctr_sys.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(test_led, LOG_LEVEL_DBG);

void test_led(void)
{
	LOG_INF("Init");

#if 1
	ctr_bsp_set_led(CTR_BSP_LED_R, true);
	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));
	ctr_bsp_set_led(CTR_BSP_LED_R, false);
	ctr_sys_task_sleep(CTR_SYS_MSEC(200));
#endif

#if 1
	ctr_bsp_set_led(CTR_BSP_LED_G, true);
	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));
	ctr_bsp_set_led(CTR_BSP_LED_G, false);
	ctr_sys_task_sleep(CTR_SYS_MSEC(200));
#endif

#if 1
	ctr_bsp_set_led(CTR_BSP_LED_Y, true);
	ctr_sys_task_sleep(CTR_SYS_MSEC(1000));
	ctr_bsp_set_led(CTR_BSP_LED_Y, false);
	ctr_sys_task_sleep(CTR_SYS_MSEC(200));
#endif
}
