#include <hio_bsp.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#define DEV_LED_R dev_gpio_1
#define PIN_LED_R 13

#define DEV_LED_G dev_gpio_1
#define PIN_LED_G 11

#define DEV_LED_Y dev_gpio_1
#define PIN_LED_Y 12

#define DEV_LED_EXT dev_gpio_1
#define PIN_LED_EXT 4

#define DEV_RF_INT dev_gpio_0
#define PIN_RF_INT 9

#define DEV_RF_EXT dev_gpio_0
#define PIN_RF_EXT 10

#define DEV_RF_LTE dev_gpio_0
#define PIN_RF_LTE 25

#define DEV_RF_LORA dev_gpio_1
#define PIN_RF_LORA 2

#define DEV_LTE_WKUP dev_gpio_0
#define PIN_LTE_WKUP 15

static const struct device *dev_gpio_0;
static const struct device *dev_gpio_1;

int
hio_bsp_init(void)
{
    dev_gpio_0 = device_get_binding("GPIO_0");
    dev_gpio_1 = device_get_binding("GPIO_1");

    if (gpio_pin_configure(DEV_LED_R, PIN_LED_R,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -1;
    }

    if (gpio_pin_configure(DEV_LED_G, PIN_LED_G,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -2;
    }

    if (gpio_pin_configure(DEV_LED_Y, PIN_LED_Y,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -3;
    }

    if (gpio_pin_configure(DEV_LED_EXT, PIN_LED_EXT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -4;
    }

    if (gpio_pin_configure(DEV_RF_INT, PIN_RF_INT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -5;
    }

    if (gpio_pin_configure(DEV_RF_EXT, PIN_RF_EXT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -6;
    }

    if (gpio_pin_configure(DEV_RF_LTE, PIN_RF_LTE,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -7;
    }

    if (gpio_pin_configure(DEV_RF_LORA, PIN_RF_LORA,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -8;
    }

    if (gpio_pin_configure(DEV_LTE_WKUP, PIN_LTE_WKUP,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        return -9;
    }

    return 0;
}

int
hio_bsp_set_rf_ant(hio_bsp_rf_ant_t ant)
{
    switch (ant) {
    case HIO_BSP_RF_ANT_NONE:
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 0) < 0) {
            return -1;
        }
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 0) < 0) {
            return -2;
        }
        break;

    case HIO_BSP_RF_ANT_INT:
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 0) < 0) {
            return -3;
        }
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 1) < 0) {
            return -4;
        }
        break;

    case HIO_BSP_RF_ANT_EXT:
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 0) < 0) {
            return -5;
        }
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 1) < 0) {
            return -6;
        }
        break;

    default:
        return -7;
    }

    return 0;
}

int
hio_bsp_set_rf_mux(hio_bsp_rf_mux_t mux)
{
    switch (mux) {
    case HIO_BSP_RF_MUX_NONE:
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 0) < 0) {
            return -1;
        }
        if (gpio_pin_set(DEV_RF_LORA, PIN_RF_LORA, 0) < 0) {
            return -2;
        }
        break;

    case HIO_BSP_RF_MUX_LTE:
        if (gpio_pin_set(DEV_RF_LORA, PIN_RF_LORA, 0) < 0) {
            return -3;
        }
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 1) < 0) {
            return -4;
        }
        break;

    case HIO_BSP_RF_MUX_LORA:
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 0) < 0) {
            return -5;
        }
        if (gpio_pin_set(DEV_RF_LORA, PIN_RF_LORA, 1) < 0) {
            return -6;
        }
        break;

    default:
        return -7;
    }

    return 0;
}

int
hio_bsp_set_led(hio_bsp_led_t led, bool on)
{
    switch (led) {
    case HIO_BSP_LED_R:
        if (gpio_pin_set(DEV_LED_R, PIN_LED_R, on ? 1 : 0) < 0) {
            return -1;
        }
        break;

    case HIO_BSP_LED_G:
        if (gpio_pin_set(DEV_LED_G, PIN_LED_G, on ? 1 : 0) < 0) {
            return -2;
        }
        break;

    case HIO_BSP_LED_Y:
        if (gpio_pin_set(DEV_LED_Y, PIN_LED_Y, on ? 1 : 0) < 0) {
            return -3;
        }
        break;

    case HIO_BSP_LED_EXT:
        if (gpio_pin_set(DEV_LED_EXT, PIN_LED_EXT, on ? 1 : 0) < 0) {
            return -4;
        }
        break;

    default:
        return -5;
    }

    return 0;
}

int
hio_bsp_set_lte_wkup(int level)
{
    if (gpio_pin_set(DEV_LTE_WKUP, PIN_LTE_WKUP, level == 0 ? 0 : 1) < 0) {
        return -1;
    }

    return 0;
}
