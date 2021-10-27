#ifndef CHESTER_INCLUDE_BUS_I2C_H_
#define CHESTER_INCLUDE_BUS_I2C_H_

/* Zephyr includes */
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTR_BUS_I2C_MEM_ADDR_16B 0x80000000

struct ctr_bus_i2c_xfer {
	uint8_t dev_addr;
	void *buf;
	size_t len;
};

struct ctr_bus_i2c_mem_xfer {
	uint8_t dev_addr;
	uint32_t mem_addr;
	void *buf;
	size_t len;
};

typedef int (*ctr_bus_i2c_init_cb)(void *ctx);
typedef int (*ctr_bus_i2c_enable_cb)(void *ctx);
typedef int (*ctr_bus_i2c_disable_cb)(void *ctx);
typedef int (*ctr_bus_i2c_read_cb)(void *ctx, const struct ctr_bus_i2c_xfer *xfer);
typedef int (*ctr_bus_i2c_write_cb)(void *ctx, const struct ctr_bus_i2c_xfer *xfer);
typedef int (*ctr_bus_i2c_mem_read_cb)(void *ctx, const struct ctr_bus_i2c_mem_xfer *xfer);
typedef int (*ctr_bus_i2c_mem_write_cb)(void *ctx, const struct ctr_bus_i2c_mem_xfer *xfer);

struct ctr_bus_i2c_driver {
	ctr_bus_i2c_init_cb init;
	ctr_bus_i2c_enable_cb enable;
	ctr_bus_i2c_disable_cb disable;
	ctr_bus_i2c_read_cb read;
	ctr_bus_i2c_write_cb write;
	ctr_bus_i2c_mem_read_cb mem_read;
	ctr_bus_i2c_mem_write_cb mem_write;
};

struct ctr_bus_i2c {
	const struct ctr_bus_i2c_driver *drv;
	void *drv_ctx;
	struct k_mutex mut;
	int acq_cnt;
};

int ctr_bus_i2c_init(struct ctr_bus_i2c *ctx, const struct ctr_bus_i2c_driver *drv, void *drv_ctx);
int ctr_bus_i2c_acquire(struct ctr_bus_i2c *ctx);
int ctr_bus_i2c_release(struct ctr_bus_i2c *ctx);
int ctr_bus_i2c_read(struct ctr_bus_i2c *ctx, const struct ctr_bus_i2c_xfer *xfer);
int ctr_bus_i2c_write(struct ctr_bus_i2c *ctx, const struct ctr_bus_i2c_xfer *xfer);
int ctr_bus_i2c_mem_read(struct ctr_bus_i2c *ctx, const struct ctr_bus_i2c_mem_xfer *xfer);
int ctr_bus_i2c_mem_write(struct ctr_bus_i2c *ctx, const struct ctr_bus_i2c_mem_xfer *xfer);
int ctr_bus_i2c_mem_read_8b(struct ctr_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                            uint8_t *data);
int ctr_bus_i2c_mem_read_16b(struct ctr_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                             uint16_t *data);
int ctr_bus_i2c_mem_write_8b(struct ctr_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                             uint8_t data);
int ctr_bus_i2c_mem_write_16b(struct ctr_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                              uint16_t data);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_BUS_I2C_H_ */
