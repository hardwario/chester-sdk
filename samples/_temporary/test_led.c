#include <hio_bsp.h>
#include <hio_sys.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(test_led, LOG_LEVEL_DBG);

void test_led(void)
{
	LOG_INF("Init");

#if 1
	hio_bsp_set_led(HIO_BSP_LED_R, true);
	hio_sys_task_sleep(HIO_SYS_MSEC(1000));
	hio_bsp_set_led(HIO_BSP_LED_R, false);
	hio_sys_task_sleep(HIO_SYS_MSEC(200));
#endif

#if 1
	hio_bsp_set_led(HIO_BSP_LED_G, true);
	hio_sys_task_sleep(HIO_SYS_MSEC(1000));
	hio_bsp_set_led(HIO_BSP_LED_G, false);
	hio_sys_task_sleep(HIO_SYS_MSEC(200));
#endif

#if 1
	hio_bsp_set_led(HIO_BSP_LED_Y, true);
	hio_sys_task_sleep(HIO_SYS_MSEC(1000));
	hio_bsp_set_led(HIO_BSP_LED_Y, false);
	hio_sys_task_sleep(HIO_SYS_MSEC(200));
#endif
}
