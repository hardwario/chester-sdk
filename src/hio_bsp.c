#include <hio_bsp.h>
#include <hio_bsp_i2c.h>
#include <hio_log.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#define HIO_LOG_ENABLED 1
#define HIO_LOG_PREFIX "HIO:BSP"

#define DEV_LED_R dev_gpio_1
#define PIN_LED_R 13

#define DEV_LED_G dev_gpio_1
#define PIN_LED_G 11

#define DEV_LED_Y dev_gpio_1
#define PIN_LED_Y 12

#define DEV_LED_EXT dev_gpio_1
#define PIN_LED_EXT 4

#define DEV_BTN_INT dev_gpio_0
#define PIN_BTN_INT 27

#define DEV_BTN_EXT dev_gpio_0
#define PIN_BTN_EXT 26

#define DEV_BAT_LOAD dev_gpio_1
#define PIN_BAT_LOAD 14

#define DEV_SLPZ dev_gpio_1
#define PIN_SLPZ 10

#define DEV_GNSS_ON dev_gpio_1
#define PIN_GNSS_ON 1

#define DEV_GNSS_RTC dev_gpio_1
#define PIN_GNSS_RTC 3

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

static const struct device *dev_i2c;
static const struct device *dev_gpio_0;
static const struct device *dev_gpio_1;

static hio_bus_i2c_t i2c;

int
hio_bsp_init(void)
{
    dev_i2c = device_get_binding("I2C_0");

    if (dev_i2c == NULL) {
        hio_log_fatal("Call `device_get_binding` failed");
        return -1;
    }

    const hio_bus_i2c_driver_t *i2c_drv = hio_bsp_i2c_get_driver();

    if (hio_bus_i2c_init(&i2c, i2c_drv, (void *)dev_i2c) < 0) {
        hio_log_fatal("Call `hio_bus_i2c_init` failed");
        return -2;
    }

    dev_gpio_0 = device_get_binding("GPIO_0");

    if (dev_gpio_0 == NULL) {
        hio_log_fatal("Call `device_get_binding` failed");
        return -3;
    }

    dev_gpio_1 = device_get_binding("GPIO_1");

    if (dev_gpio_1 == NULL) {
        hio_log_fatal("Call `device_get_binding` failed");
        return -4;
    }

    if (gpio_pin_configure(DEV_LED_R, PIN_LED_R,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -5;
    }

    if (gpio_pin_configure(DEV_LED_G, PIN_LED_G,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -6;
    }

    if (gpio_pin_configure(DEV_LED_Y, PIN_LED_Y,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -7;
    }

    if (gpio_pin_configure(DEV_LED_EXT, PIN_LED_EXT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -8;
    }

    if (gpio_pin_configure(DEV_BTN_INT, PIN_BTN_INT,
                           GPIO_INPUT | GPIO_PULL_UP) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -9;
    }

    if (gpio_pin_configure(DEV_BTN_EXT, PIN_BTN_EXT,
                           GPIO_INPUT | GPIO_PULL_UP) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -10;
    }

    if (gpio_pin_configure(DEV_BAT_LOAD, PIN_BAT_LOAD,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -11;
    }

    if (gpio_pin_configure(DEV_SLPZ, PIN_SLPZ,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -12;
    }

    if (gpio_pin_configure(DEV_GNSS_ON, PIN_GNSS_ON,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -13;
    }

    if (gpio_pin_configure(DEV_GNSS_RTC, PIN_GNSS_RTC,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -14;
    }

    if (gpio_pin_configure(DEV_RF_INT, PIN_RF_INT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -15;
    }

    if (gpio_pin_configure(DEV_RF_EXT, PIN_RF_EXT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -16;
    }

    if (gpio_pin_configure(DEV_RF_LTE, PIN_RF_LTE,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -17;
    }

    if (gpio_pin_configure(DEV_RF_LORA, PIN_RF_LORA,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -18;
    }

    if (gpio_pin_configure(DEV_LTE_WKUP, PIN_LTE_WKUP,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fatal("Call `gpio_pin_configure` failed");
        return -19;
    }

    return 0;
}

hio_bus_i2c_t *
hio_bsp_get_i2c(void)
{
    return &i2c;
}

int
hio_bsp_set_led(hio_bsp_led_t led, bool on)
{
    switch (led) {
    case HIO_BSP_LED_R:
        if (gpio_pin_set(DEV_LED_R, PIN_LED_R, on ? 1 : 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -1;
        }
        break;

    case HIO_BSP_LED_G:
        if (gpio_pin_set(DEV_LED_G, PIN_LED_G, on ? 1 : 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -2;
        }
        break;

    case HIO_BSP_LED_Y:
        if (gpio_pin_set(DEV_LED_Y, PIN_LED_Y, on ? 1 : 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -3;
        }
        break;

    case HIO_BSP_LED_EXT:
        if (gpio_pin_set(DEV_LED_EXT, PIN_LED_EXT, on ? 1 : 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -4;
        }
        break;

    default:
        hio_log_fatal("Invalid parameter");
        return -5;
    }

    return 0;
}

int
hio_bsp_get_button(hio_bsp_button_t button, bool *pressed)
{
    int ret;

    switch (button) {
    case HIO_BSP_BUTTON_INT:
        ret = gpio_pin_get(DEV_BTN_INT, PIN_BTN_INT);
        if (ret < 0) {
            hio_log_fatal("Call `gpio_pin_get` failed");
            return -1;
        }
        *pressed = ret == 0 ? true : false;
        break;
    case HIO_BSP_BUTTON_EXT:
        ret = gpio_pin_get(DEV_BTN_EXT, PIN_BTN_EXT);
        if (ret < 0) {
            hio_log_fatal("Call `gpio_pin_get` failed");
            return -2;
        }
        *pressed = ret == 0 ? true : false;
        break;
    default:
        hio_log_fatal("Invalid parameter");
        return -3;
    }

    return 0;
}

int
hio_bsp_set_bat_load(bool on)
{
    if (gpio_pin_set(DEV_BAT_LOAD, PIN_BAT_LOAD, on ? 1 : 0) < 0) {
        hio_log_fatal("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_gnss_on(bool on)
{
    if (gpio_pin_set(DEV_GNSS_ON, PIN_GNSS_ON, on ? 1 : 0) < 0) {
        hio_log_fatal("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_gnss_rtc(bool on)
{
    if (gpio_pin_set(DEV_GNSS_RTC, PIN_GNSS_RTC, on ? 1 : 0) < 0) {
        hio_log_fatal("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_slpz(int level)
{
    if (gpio_pin_set(DEV_SLPZ, PIN_SLPZ, level == 0 ? 0 : 1) < 0) {
        hio_log_fatal("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_rf_ant(hio_bsp_rf_ant_t ant)
{
    switch (ant) {
    case HIO_BSP_RF_ANT_NONE:
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -1;
        }
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -2;
        }
        break;

    case HIO_BSP_RF_ANT_INT:
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -3;
        }
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 1) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -4;
        }
        break;

    case HIO_BSP_RF_ANT_EXT:
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -5;
        }
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 1) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -6;
        }
        break;

    default:
        hio_log_fatal("Invalid parameter");
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
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -1;
        }
        if (gpio_pin_set(DEV_RF_LORA, PIN_RF_LORA, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -2;
        }
        break;

    case HIO_BSP_RF_MUX_LTE:
        if (gpio_pin_set(DEV_RF_LORA, PIN_RF_LORA, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -3;
        }
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 1) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -4;
        }
        break;

    case HIO_BSP_RF_MUX_LORA:
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 0) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -5;
        }
        if (gpio_pin_set(DEV_RF_LORA, PIN_RF_LORA, 1) < 0) {
            hio_log_fatal("Call `gpio_pin_set` failed");
            return -6;
        }
        break;

    default:
        hio_log_fatal("Invalid parameter");
        return -7;
    }

    return 0;
}

int
hio_bsp_set_lte_wkup(int level)
{
    if (gpio_pin_set(DEV_LTE_WKUP, PIN_LTE_WKUP, level == 0 ? 0 : 1) < 0) {
        hio_log_fatal("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}
