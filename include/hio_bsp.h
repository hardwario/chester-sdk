#ifndef HIO_BSP_H
#define HIO_BSP_H

#include <hio_bus_i2c.h>

// Zephyr includes
#include <devicetree.h>

// Standard includes
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIO_BSP_GP0A_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP0A_PIN 3
#define HIO_BSP_GP1A_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP1A_PIN 29
#define HIO_BSP_GP2A_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP2A_PIN 2
#define HIO_BSP_GP3A_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP3A_PIN 31

#define HIO_BSP_GP0B_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP0B_PIN 28
#define HIO_BSP_GP1B_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP1B_PIN 30
#define HIO_BSP_GP2B_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP2B_PIN 4
#define HIO_BSP_GP3B_DEV DT_LABEL(DT_NODELABEL(gpio0))
#define HIO_BSP_GP3B_PIN 5

enum hio_bsp_led {
	HIO_BSP_LED_R = 0,
	HIO_BSP_LED_G = 1,
	HIO_BSP_LED_Y = 2,
	HIO_BSP_LED_EXT = 3,
};

enum hio_bsp_button {
	HIO_BSP_BUTTON_INT = 0,
	HIO_BSP_BUTTON_EXT = 1,
};

enum hio_bsp_rf_ant {
	HIO_BSP_RF_ANT_NONE = 0,
	HIO_BSP_RF_ANT_INT = 1,
	HIO_BSP_RF_ANT_EXT = 2,
};

enum hio_bsp_rf_mux {
	HIO_BSP_RF_MUX_NONE = 0,
	HIO_BSP_RF_MUX_LTE = 1,
	HIO_BSP_RF_MUX_LRW = 2,
};

struct hio_bus_i2c *hio_bsp_get_i2c(void);
int hio_bsp_set_led(enum hio_bsp_led led, bool on);
int hio_bsp_get_button(enum hio_bsp_button button, bool *pressed);
int hio_bsp_set_batt_load(bool on);
int hio_bsp_set_batt_test(bool on);
int hio_bsp_set_gnss_on(bool on);
int hio_bsp_set_gnss_rtc(bool on);
int hio_bsp_set_w1b_slpz(int level);
int hio_bsp_set_rf_ant(enum hio_bsp_rf_ant ant);
int hio_bsp_set_rf_mux(enum hio_bsp_rf_mux mux);
int hio_bsp_set_lte_reset(int level);
int hio_bsp_set_lte_wkup(int level);
int hio_bsp_set_lrw_reset(int level);
int hio_bsp_sht30_measure(float *t, float *rh);
int hio_bsp_tmp112_measure(float *t);

#ifdef __cplusplus
}
#endif

#endif
