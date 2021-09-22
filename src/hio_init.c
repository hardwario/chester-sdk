#include <hio_bsp.h>
#include <hio_bsp_i2c.h>
#include <hio_bus_i2c.h>
#include <hio_sys.h>
#include <hio_test.h>

// Zephyr includes
#include <init.h>
#include <logging/log.h>
#include <settings/settings.h>
#include <zephyr.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>

LOG_MODULE_REGISTER(hio_init, LOG_LEVEL_DBG);

static hio_sys_task_t button_task;
static HIO_SYS_TASK_STACK_DEFINE(button_task_stack, 2048);

static void button_task_entry(void *param)
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

            LOG_INF("Discharge request - load ON");

            hio_bsp_set_led(HIO_BSP_LED_G, true);
            hio_bsp_set_batt_load(true);

            k_sleep(K_SECONDS(120));

            hio_bsp_set_batt_load(false);
            hio_bsp_set_led(HIO_BSP_LED_G, false);

            LOG_INF("Discharge request - load OFF");
        }

        k_sleep(K_MSEC(50));
    }
}

static void wait_for_button(void)
{
    hio_bsp_set_led(HIO_BSP_LED_Y, true);

    for (;;) {
        bool pressed;
        if (hio_bsp_get_button(HIO_BSP_BUTTON_INT, &pressed) == 0) {
            if (pressed) {
                break;
            }
        }

        k_sleep(K_MSEC(100));
    }

    hio_bsp_set_led(HIO_BSP_LED_Y, false);
}

static int init(const struct device *dev)
{
    ARG_UNUSED(dev);

    int ret;

    #if 0
    for (;;) {
        hio_sys_task_sleep(HIO_SYS_SECONDS(5));
    }
    #endif

    ret = settings_subsys_init();

    if (ret < 0) {
        LOG_ERR("Call `settings_subsys_init` failed: %d", ret);
        return ret;
    }

    ret = hio_bsp_init();

    if (ret < 0) {
        LOG_ERR("Call `hio_bsp_init` failed: %d", ret);
        return ret;
    }

    hio_sys_task_init(&button_task, "button", button_task_stack,
                      HIO_SYS_TASK_STACK_SIZEOF(button_task_stack),
                      button_task_entry, NULL);

    #if 1
    wait_for_button();
    #endif

    #if 1
    if (hio_test_is_active()) {
        return 1;
    }

    LOG_INF("Waiting for test mode");

    k_sleep(K_SECONDS(10));

    if (hio_test_is_active()) {
        return 1;
    }

    hio_test_block();

    LOG_INF("Test mode entry timed out");
    #endif

    return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
