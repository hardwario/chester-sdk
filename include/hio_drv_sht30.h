#ifndef HIO_DRV_SHT30_H
#define HIO_DRV_SHT30_H

#include <hio_bus_i2c.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    hio_bus_i2c_t *i2c;
    uint8_t dev_addr;
    bool ready;
} hio_drv_sht30_t;

int
hio_drv_sht30_init(hio_drv_sht30_t *ctx, hio_bus_i2c_t *i2c, uint8_t dev_addr);

int
hio_drv_sht30_measure(hio_drv_sht30_t *ctx, float *t, float *rh);

#endif
