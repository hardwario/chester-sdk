#ifndef HIO_BUS_I2C_H
#define HIO_BUS_I2C_H

#include <hio_sys.h>

// Standard includes
#include <stddef.h>
#include <stdint.h>

#define HIO_BUS_I2C_MEM_ADDR_16B 0x80000000

typedef struct {
    uint8_t dev_addr;
    void *buf;
    size_t len;
} hio_bus_i2c_xfer_t;

typedef struct {
    uint8_t dev_addr;
    uint32_t mem_addr;
    void *buf;
    size_t len;
} hio_bus_i2c_mem_xfer_t;

typedef int (*hio_bus_i2c_init_t)(void *ctx);
typedef int (*hio_bus_i2c_enable_t)(void *ctx);
typedef int (*hio_bus_i2c_disable_t)(void *ctx);
typedef int (*hio_bus_i2c_read_t)(void *ctx,
                                  const hio_bus_i2c_xfer_t *xfer);
typedef int (*hio_bus_i2c_write_t)(void *ctx,
                                   const hio_bus_i2c_xfer_t *xfer);
typedef int (*hio_bus_i2c_mem_read_t)(void *ctx,
                                      const hio_bus_i2c_mem_xfer_t *xfer);
typedef int (*hio_bus_i2c_mem_write_t)(void *ctx,
                                       const hio_bus_i2c_mem_xfer_t *xfer);

typedef struct {
    hio_bus_i2c_init_t init;
    hio_bus_i2c_enable_t enable;
    hio_bus_i2c_disable_t disable;
    hio_bus_i2c_read_t read;
    hio_bus_i2c_write_t write;
    hio_bus_i2c_mem_read_t mem_read;
    hio_bus_i2c_mem_write_t mem_write;
} hio_bus_i2c_driver_t;

typedef struct {
    const hio_bus_i2c_driver_t *drv;
    void *drv_ctx;
    hio_sys_mut_t mut;
    int acq_cnt;
} hio_bus_i2c_t;

int
hio_bus_i2c_init(hio_bus_i2c_t *ctx,
                 const hio_bus_i2c_driver_t *drv, void *drv_ctx);

int
hio_bus_i2c_acquire(hio_bus_i2c_t *ctx);

int
hio_bus_i2c_release(hio_bus_i2c_t *ctx);

int
hio_bus_i2c_read(hio_bus_i2c_t *ctx,
                 const hio_bus_i2c_xfer_t *xfer);

int
hio_bus_i2c_write(hio_bus_i2c_t *ctx,
                  const hio_bus_i2c_xfer_t *xfer);

int
hio_bus_i2c_mem_read(hio_bus_i2c_t *ctx,
                     const hio_bus_i2c_mem_xfer_t *xfer);

int
hio_bus_i2c_mem_write(hio_bus_i2c_t *ctx,
                      const hio_bus_i2c_mem_xfer_t *xfer);

int
hio_bus_i2c_mem_read_8b(hio_bus_i2c_t *ctx,
                        uint8_t dev_addr, uint32_t mem_addr, uint8_t *data);

int
hio_bus_i2c_mem_read_16b(hio_bus_i2c_t *ctx,
                         uint8_t dev_addr, uint32_t mem_addr, uint16_t *data);

int
hio_bus_i2c_mem_write_8b(hio_bus_i2c_t *ctx,
                         uint8_t dev_addr, uint32_t mem_addr, uint8_t data);

int
hio_bus_i2c_mem_write_16b(hio_bus_i2c_t *ctx,
                          uint8_t dev_addr, uint32_t mem_addr, uint16_t data);

#endif
