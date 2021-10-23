#ifndef CHESTER_INCLUDE_BUS_I2C_H_
#define CHESTER_INCLUDE_BUS_I2C_H_

// Zephyr includes
#include <zephyr.h>

// Standard includes
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIO_BUS_I2C_MEM_ADDR_16B 0x80000000

struct hio_bus_i2c_xfer {
	uint8_t dev_addr;
	void *buf;
	size_t len;
};

struct hio_bus_i2c_mem_xfer {
	uint8_t dev_addr;
	uint32_t mem_addr;
	void *buf;
	size_t len;
};

typedef int (*hio_bus_i2c_init_cb)(void *ctx);
typedef int (*hio_bus_i2c_enable_cb)(void *ctx);
typedef int (*hio_bus_i2c_disable_cb)(void *ctx);
typedef int (*hio_bus_i2c_read_cb)(void *ctx, const struct hio_bus_i2c_xfer *xfer);
typedef int (*hio_bus_i2c_write_cb)(void *ctx, const struct hio_bus_i2c_xfer *xfer);
typedef int (*hio_bus_i2c_mem_read_cb)(void *ctx, const struct hio_bus_i2c_mem_xfer *xfer);
typedef int (*hio_bus_i2c_mem_write_cb)(void *ctx, const struct hio_bus_i2c_mem_xfer *xfer);

struct hio_bus_i2c_driver {
	hio_bus_i2c_init_cb init;
	hio_bus_i2c_enable_cb enable;
	hio_bus_i2c_disable_cb disable;
	hio_bus_i2c_read_cb read;
	hio_bus_i2c_write_cb write;
	hio_bus_i2c_mem_read_cb mem_read;
	hio_bus_i2c_mem_write_cb mem_write;
};

struct hio_bus_i2c {
	const struct hio_bus_i2c_driver *drv;
	void *drv_ctx;
	struct k_mutex mut;
	int acq_cnt;
};

int hio_bus_i2c_init(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_driver *drv, void *drv_ctx);
int hio_bus_i2c_acquire(struct hio_bus_i2c *ctx);
int hio_bus_i2c_release(struct hio_bus_i2c *ctx);
int hio_bus_i2c_read(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_xfer *xfer);
int hio_bus_i2c_write(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_xfer *xfer);
int hio_bus_i2c_mem_read(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_mem_xfer *xfer);
int hio_bus_i2c_mem_write(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_mem_xfer *xfer);
int hio_bus_i2c_mem_read_8b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                            uint8_t *data);
int hio_bus_i2c_mem_read_16b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                             uint16_t *data);
int hio_bus_i2c_mem_write_8b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                             uint8_t data);
int hio_bus_i2c_mem_write_16b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                              uint16_t data);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_BUS_I2C_H_ */
