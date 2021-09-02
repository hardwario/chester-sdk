#include <hio_bsp.h>
#include <hio_bsp_i2c.h>
#include <hio_drv_sht30.h>
#include <hio_drv_tmp112.h>
#include <hio_log.h>
#include <hio_rtc.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>

HIO_LOG_REGISTER(hio_bsp, HIO_LOG_LEVEL_DBG);

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

#define DEV_RF_LRW dev_gpio_1
#define PIN_RF_LRW 2

#define DEV_LTE_RESET dev_gpio_1
#define PIN_LTE_RESET 7

#define DEV_LTE_WKUP dev_gpio_0
#define PIN_LTE_WKUP 15

#define DEV_LRW_RESET dev_gpio_0
#define PIN_LRW_RESET 11

static const struct device *dev_gpio_0;
static const struct device *dev_gpio_1;
static const struct device *dev_i2c_0;
static const struct device *dev_spi_1;

static hio_bus_i2c_t i2c;

static hio_drv_sht30_t sht30;
static hio_drv_tmp112_t tmp112;

static int
init_rtc(void)
{
    if (hio_rtc_init() < 0) {
        hio_log_fat("Call `hio_rtc_init` failed");
        return -1;
    }

    return 0;
}

static int
init_gpio(void)
{
    dev_gpio_0 = device_get_binding("GPIO_0");

    if (dev_gpio_0 == NULL) {
        hio_log_fat("Call `device_get_binding` failed");
        return -1;
    }

    dev_gpio_1 = device_get_binding("GPIO_1");

    if (dev_gpio_1 == NULL) {
        hio_log_fat("Call `device_get_binding` failed");
        return -2;
    }

    return 0;
}

static int
init_led(void)
{
    if (gpio_pin_configure(DEV_LED_R, PIN_LED_R,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    if (gpio_pin_configure(DEV_LED_G, PIN_LED_G,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -2;
    }

    if (gpio_pin_configure(DEV_LED_Y, PIN_LED_Y,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -3;
    }

    if (gpio_pin_configure(DEV_LED_EXT, PIN_LED_EXT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -4;
    }

    return 0;
}

static int
init_button(void)
{
    if (gpio_pin_configure(DEV_BTN_INT, PIN_BTN_INT,
                           GPIO_INPUT | GPIO_PULL_UP) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    if (gpio_pin_configure(DEV_BTN_EXT, PIN_BTN_EXT,
                           GPIO_INPUT | GPIO_PULL_UP) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -2;
    }

    return 0;
}

static int
init_i2c(void)
{
    dev_i2c_0 = device_get_binding("I2C_0");

    if (dev_i2c_0 == NULL) {
        hio_log_fat("Call `device_get_binding` failed");
        return -1;
    }

    const hio_bus_i2c_driver_t *i2c_drv = hio_bsp_i2c_get_driver();

    if (hio_bus_i2c_init(&i2c, i2c_drv, (void *)dev_i2c_0) < 0) {
        hio_log_fat("Call `hio_bus_i2c_init` failed");
        return -2;
    }

    if (hio_drv_sht30_init(&sht30, &i2c, 0x45) < 0) {
        hio_log_fat("Call `hio_drv_sht30_init` failed");
        return -3;
    }

    if (hio_drv_tmp112_init(&tmp112, &i2c, 0x48) < 0) {
        hio_log_fat("Call `hio_drv_tmp112_init` failed");
        return -4;
    }

    return 0;
}

static int
init_tmp112(void)
{
    if (hio_drv_tmp112_sleep(&tmp112) < 0) {
        hio_log_err("Call `hio_drv_tmp112_sleep` failed");
        return -1;
    }

    return 0;
}

static int
init_flash(void)
{
    dev_spi_1 = device_get_binding("SPI_1");

    if (dev_spi_1 == NULL) {
        hio_log_fat("Call `device_get_binding` failed");
        return -1;
    }

    struct spi_cs_control spi_cs = {0};

    spi_cs.gpio_dev = dev_gpio_0;
    spi_cs.gpio_pin = 23;
    spi_cs.gpio_dt_flags = GPIO_ACTIVE_LOW;
    spi_cs.delay = 2;

    struct spi_config spi_cfg = {0};

    spi_cfg.frequency = 500000;
    spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE;
    spi_cfg.cs = &spi_cs;

    uint8_t buffer_tx[] = { 0x79 };

    const struct spi_buf tx_buf[] = {
        {
            .buf = buffer_tx,
            .len = sizeof(buffer_tx)
        }
    };

    const struct spi_buf_set tx = {
        .buffers = tx_buf,
        .count = 1
    };

    const struct spi_buf rx_buf[] = {
        {
            .buf = NULL,
            .len = 1
        }
    };

    const struct spi_buf_set rx = {
        .buffers = rx_buf,
        .count = 1
    };

    if (spi_transceive(dev_spi_1, &spi_cfg, &tx, &rx) < 0) {
        hio_log_fat("Call `spi_transceive` failed");
        return -2;
    }

    return 0;
}

int
init_rf_mux(void)
{
    if (gpio_pin_configure(DEV_RF_INT, PIN_RF_INT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    if (gpio_pin_configure(DEV_RF_EXT, PIN_RF_EXT,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -2;
    }

    if (gpio_pin_configure(DEV_RF_LTE, PIN_RF_LTE,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -3;
    }

    if (gpio_pin_configure(DEV_RF_LRW, PIN_RF_LRW,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -4;
    }

    return 0;
}

int
init_gnss(void)
{
    if (gpio_pin_configure(DEV_GNSS_ON, PIN_GNSS_ON,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    if (gpio_pin_configure(DEV_GNSS_RTC, PIN_GNSS_RTC,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -2;
    }

    return 0;
}

int
init_lte(void)
{
    if (gpio_pin_configure(DEV_LTE_RESET, PIN_LTE_RESET,
                           GPIO_DISCONNECTED) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    if (gpio_pin_configure(DEV_LTE_WKUP, PIN_LTE_WKUP,
                           GPIO_OUTPUT_ACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    return 0;
}

int
init_bat_test(void)
{
    if (gpio_pin_configure(DEV_BAT_LOAD, PIN_BAT_LOAD,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    return 0;
}

int
init_w1b(void)
{
    if (gpio_pin_configure(DEV_SLPZ, PIN_SLPZ,
                           GPIO_OUTPUT_INACTIVE) < 0) {
        hio_log_fat("Call `gpio_pin_configure` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_init(void)
{
    if (init_rtc() < 0) {
        hio_log_fat("Call `init_i2c` failed");
        return -1;
    }

    if (init_gpio() < 0) {
        hio_log_fat("Call `init_gpio` failed");
        return -2;
    }

    if (init_led() < 0) {
        hio_log_fat("Call `init_led` failed");
        return -3;
    }

    if (init_button() < 0) {
        hio_log_fat("Call `init_button` failed");
        return -4;
    }

    if (init_i2c() < 0) {
        hio_log_fat("Call `init_i2c` failed");
        return -1;
    }

    if (init_tmp112() < 0) {
        hio_log_fat("Call `init_tmp112` failed");
        return -6;
    }

    if (init_flash() < 0) {
        hio_log_fat("Call `init_flash` failed");
        return -5;
    }

    if (init_rf_mux() < 0) {
        hio_log_fat("Call `init_rf_mux` failed");
        return -7;
    }

    if (init_gnss() < 0) {
        hio_log_fat("Call `init_gnss` failed");
        return -8;
    }

    if (init_lte() < 0) {
        hio_log_fat("Call `init_lte` failed");
        return -9;
    }

    if (init_bat_test() < 0) {
        hio_log_fat("Call `init_bat_test` failed");
        return -10;
    }

    if (init_w1b() < 0) {
        hio_log_fat("Call `init_w1b` failed");
        return -11;
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
            hio_log_fat("Call `gpio_pin_set` failed");
            return -1;
        }
        break;

    case HIO_BSP_LED_G:
        if (gpio_pin_set(DEV_LED_G, PIN_LED_G, on ? 1 : 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -2;
        }
        break;

    case HIO_BSP_LED_Y:
        if (gpio_pin_set(DEV_LED_Y, PIN_LED_Y, on ? 1 : 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -3;
        }
        break;

    case HIO_BSP_LED_EXT:
        if (gpio_pin_set(DEV_LED_EXT, PIN_LED_EXT, on ? 1 : 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -4;
        }
        break;

    default:
        hio_log_fat("Invalid parameter");
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
            hio_log_fat("Call `gpio_pin_get` failed");
            return -1;
        }
        *pressed = ret == 0 ? true : false;
        break;
    case HIO_BSP_BUTTON_EXT:
        ret = gpio_pin_get(DEV_BTN_EXT, PIN_BTN_EXT);
        if (ret < 0) {
            hio_log_fat("Call `gpio_pin_get` failed");
            return -2;
        }
        *pressed = ret == 0 ? true : false;
        break;
    default:
        hio_log_fat("Invalid parameter");
        return -3;
    }

    return 0;
}

int
hio_bsp_set_bat_load(bool on)
{
    if (gpio_pin_set(DEV_BAT_LOAD, PIN_BAT_LOAD, on ? 1 : 0) < 0) {
        hio_log_fat("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_gnss_on(bool on)
{
    if (gpio_pin_set(DEV_GNSS_ON, PIN_GNSS_ON, on ? 1 : 0) < 0) {
        hio_log_fat("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_gnss_rtc(bool on)
{
    if (gpio_pin_set(DEV_GNSS_RTC, PIN_GNSS_RTC, on ? 1 : 0) < 0) {
        hio_log_fat("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_w1b_slpz(int level)
{
    if (gpio_pin_set(DEV_SLPZ, PIN_SLPZ, level == 0 ? 0 : 1) < 0) {
        hio_log_fat("Call `gpio_pin_set` failed");
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
            hio_log_fat("Call `gpio_pin_set` failed");
            return -1;
        }
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -2;
        }
        break;

    case HIO_BSP_RF_ANT_INT:
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -3;
        }
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 1) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -4;
        }
        break;

    case HIO_BSP_RF_ANT_EXT:
        if (gpio_pin_set(DEV_RF_INT, PIN_RF_INT, 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -5;
        }
        if (gpio_pin_set(DEV_RF_EXT, PIN_RF_EXT, 1) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -6;
        }
        break;

    default:
        hio_log_fat("Invalid parameter");
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
            hio_log_fat("Call `gpio_pin_set` failed");
            return -1;
        }
        if (gpio_pin_set(DEV_RF_LRW, PIN_RF_LRW, 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -2;
        }
        break;

    case HIO_BSP_RF_MUX_LTE:
        if (gpio_pin_set(DEV_RF_LRW, PIN_RF_LRW, 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -3;
        }
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 1) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -4;
        }
        break;

    case HIO_BSP_RF_MUX_LRW:
        if (gpio_pin_set(DEV_RF_LTE, PIN_RF_LTE, 0) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -5;
        }
        if (gpio_pin_set(DEV_RF_LRW, PIN_RF_LRW, 1) < 0) {
            hio_log_fat("Call `gpio_pin_set` failed");
            return -6;
        }
        break;

    default:
        hio_log_fat("Invalid parameter");
        return -7;
    }

    return 0;
}

int
hio_bsp_set_lte_reset(int level)
{
    if (level == 0) {
        if (gpio_pin_configure(DEV_LTE_RESET, PIN_LTE_RESET,
                               GPIO_OUTPUT_INACTIVE) < 0) {
            hio_log_fat("Call `gpio_pin_configure` failed");
            return -1;
        }
    } else {
        if (gpio_pin_configure(DEV_LTE_RESET, PIN_LTE_RESET,
                               GPIO_DISCONNECTED) < 0) {
            hio_log_fat("Call `gpio_pin_configure` failed");
            return -2;
        }
    }

    return 0;
}

int
hio_bsp_set_lte_wkup(int level)
{
    if (gpio_pin_set(DEV_LTE_WKUP, PIN_LTE_WKUP, level == 0 ? 0 : 1) < 0) {
        hio_log_fat("Call `gpio_pin_set` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_set_lrw_reset(int level)
{
    if (level == 0) {
        if (gpio_pin_configure(DEV_LRW_RESET, PIN_LRW_RESET,
                               GPIO_OUTPUT_INACTIVE) < 0) {
            hio_log_fat("Call `gpio_pin_configure` failed");
            return -1;
        }
    } else {
        if (gpio_pin_configure(DEV_LRW_RESET, PIN_LRW_RESET,
                               GPIO_DISCONNECTED) < 0) {
            hio_log_fat("Call `gpio_pin_configure` failed");
            return -2;
        }
    }

    return 0;
}

int
hio_bsp_sht30_measure(float *t, float *rh)
{
    if (hio_drv_sht30_measure(&sht30, t, rh) < 0) {
        hio_log_err("Call `hio_drv_sht30_measure` failed");
        return -1;
    }

    return 0;
}

int
hio_bsp_tmp112_measure(float *t)
{
    if (hio_drv_tmp112_measure(&tmp112, t) < 0) {
        hio_log_err("Call `hio_bsp_tmp112_measure` failed");
        return -1;
    }

    return 0;
}
