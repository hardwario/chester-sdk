#ifndef HIO_BSP_H
#define HIO_BSP_H

#include <stdbool.h>

typedef enum {
    HIO_BSP_RF_ANT_NONE = 0,
    HIO_BSP_RF_ANT_INT = 1,
    HIO_BSP_RF_ANT_EXT = 2
} hio_bsp_rf_ant_t;

typedef enum {
    HIO_BSP_RF_MUX_NONE = 0,
    HIO_BSP_RF_MUX_LTE = 1,
    HIO_BSP_RF_MUX_LORA = 2
} hio_bsp_rf_mux_t;

typedef enum {
    HIO_BSP_LED_R = 0,
    HIO_BSP_LED_G = 1,
    HIO_BSP_LED_Y = 2,
    HIO_BSP_LED_EXT = 3
} hio_bsp_led_t;

int
hio_bsp_init(void);

int
hio_bsp_set_rf_ant(hio_bsp_rf_ant_t ant);

int
hio_bsp_set_rf_mux(hio_bsp_rf_mux_t mux);

int
hio_bsp_set_led(hio_bsp_led_t led, bool on);

#endif
