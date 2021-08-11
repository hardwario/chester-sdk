#ifndef HIO_DRV_TMP112_H
#define HIO_DRV_TMP112_H

#include <hio_bus_i2c.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    hio_bus_i2c_t *i2c;
    uint8_t dev_addr;
    bool ready;
} hio_drv_tmp112_t;

int
hio_drv_tmp112_init(hio_drv_tmp112_t *ctx,
                    hio_bus_i2c_t *i2c, uint8_t dev_addr);

int
hio_drv_tmp112_measure(hio_drv_tmp112_t *ctx, float *t);

int
hio_drv_tmp112_sleep(hio_drv_tmp112_t *ctx);

#endif
