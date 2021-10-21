#ifndef HIO_DRV_TMP112_H
#define HIO_DRV_TMP112_H

#include <hio_bus_i2c.h>

// Standard includes
#include <stdbool.h>
#include <stdint.h>

struct hio_drv_tmp112 {
	struct hio_bus_i2c *i2c;
	uint8_t dev_addr;
	bool ready;
};

int hio_drv_tmp112_init(struct hio_drv_tmp112 *ctx, struct hio_bus_i2c *i2c, uint8_t dev_addr);
int hio_drv_tmp112_measure(struct hio_drv_tmp112 *ctx, float *t);
int hio_drv_tmp112_sleep(struct hio_drv_tmp112 *ctx);

#endif
