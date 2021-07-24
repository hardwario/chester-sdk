#include <application.h>

#include <hio_bsp.h>
#include <hio_bsp_i2c.h>
#include <hio_bus_i2c.h>
#include <hio_log.h>
#include <hio_sys.h>
#include <hio_test.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>

HIO_LOG_REGISTER(main, HIO_LOG_LEVEL_DBG);

static hio_sys_task_t button_task;
static HIO_SYS_TASK_STACK_DEFINE(button_task_stack, 2048);

static void
button_task_entry(void *param)
{
    for (;;) {
        int64_t now = hio_sys_uptime_get();

        static int64_t timeout;

        bool pressed;

        if (hio_bsp_get_button(HIO_BSP_BUTTON_INT, &pressed) == 0) {
            if (!pressed) {
                timeout = now + HIO_SYS_SECONDS(2);
            }
        } else {
            timeout = 0;
        }

        if (timeout != 0 && now >= timeout) {
            timeout = 0;

            hio_log_inf("Discharge request - load ON");

            hio_bsp_set_led(HIO_BSP_LED_G, true);
            hio_bsp_set_bat_load(true);
            hio_sys_task_sleep(HIO_SYS_SECONDS(120));
            hio_bsp_set_bat_load(false);
            hio_bsp_set_led(HIO_BSP_LED_G, false);

            hio_log_inf("Discharge request - load OFF");
        }

        hio_sys_task_sleep(HIO_SYS_MSEC(50));
    }
}

static void
wait_for_button(void)
{
    hio_bsp_set_led(HIO_BSP_LED_Y, true);

    for (;;) {
        bool pressed;
        if (hio_bsp_get_button(HIO_BSP_BUTTON_INT, &pressed) == 0) {
            if (pressed) {
                break;
            }
        }

        hio_sys_task_sleep(HIO_SYS_MSEC(100));
    }

    hio_bsp_set_led(HIO_BSP_LED_Y, false);
}

void
main(void)
{
    if (hio_bsp_init() < 0) {
        hio_log_fat("Call `hio_bsp_init` failed");
    }

    hio_sys_task_init(&button_task, "button", button_task_stack,
                      HIO_SYS_TASK_STACK_SIZEOF(button_task_stack),
                      button_task_entry, NULL);

    #if 1
    wait_for_button();
    #endif

    #if 1
    if (hio_test_is_active()) {
        return;
    }

    hio_log_inf("Waiting for test mode");

    hio_sys_task_sleep(HIO_SYS_SECONDS(10));

    if (hio_test_is_active()) {
        return;
    }

    hio_test_block();

    hio_log_inf("Test mode entry timed out");
    #endif

    application_init();

    for (;;) {
        application_loop();
    }
}
