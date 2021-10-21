#ifndef HIO_DRV_SHT30_H
#define HIO_DRV_SHT30_H

#include <hio_bus_i2c.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>

struct hio_drv_sht30 {
	struct hio_bus_i2c *i2c;
	uint8_t dev_addr;
	bool ready;
};

int hio_drv_sht30_init(struct hio_drv_sht30 *ctx, struct hio_bus_i2c *i2c, uint8_t dev_addr);
int hio_drv_sht30_measure(struct hio_drv_sht30 *ctx, float *t, float *rh);

#endif
