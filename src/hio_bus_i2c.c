#include <hio_bus_i2c.h>
#include <hio_log.h>

// Standard includes
#include <string.h>

HIO_LOG_REGISTER(hio_bus_i2c, HIO_LOG_LEVEL_DBG);

int
hio_bus_i2c_init(hio_bus_i2c_t *ctx,
                 const hio_bus_i2c_driver_t *drv, void *drv_ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    if (drv == NULL) {
        hio_log_fat("Driver cannot be NULL [%p]", ctx);
        return -1;
    }

    ctx->drv = drv;
    ctx->drv_ctx = drv_ctx;

    hio_sys_mut_init(&ctx->mut);

    if (ctx->drv->init != NULL) {
        if (ctx->drv->init(ctx->drv_ctx) < 0) {
            hio_log_err("Call `ctx->drv->init` failed [%p]", ctx);
            return -2;
        }
    }

    return 0;
}

int
hio_bus_i2c_acquire(hio_bus_i2c_t *ctx)
{
    hio_sys_mut_acquire(&ctx->mut);

    if ((ctx->acq_cnt)++ == 0) {
        if (ctx->drv->enable != NULL) {
            hio_log_dbg("Bus enable [%p]", ctx);

            if (ctx->drv->enable(ctx->drv_ctx) < 0) {
                hio_log_err("Call `ctx->drv->enable` failed [%p]", ctx);

                hio_sys_mut_release(&ctx->mut);
                return -1;
            }
        }
    }

    return 0;
}

int
hio_bus_i2c_release(hio_bus_i2c_t *ctx)
{
    if (ctx->acq_cnt == 0) {
        hio_log_fat("Bus not acquired [%p]", ctx);
        return -1;
    }

    if (--(ctx->acq_cnt) == 0) {
        if (ctx->drv->disable != NULL) {
            hio_log_dbg("Bus disable [%p]", ctx);

            if (ctx->drv->disable(ctx->drv_ctx) < 0) {
                hio_log_err("Call `ctx->drv->disable` failed [%p]", ctx);

                hio_sys_mut_release(&ctx->mut);
                return -2;
            }
        }
    }

    hio_sys_mut_release(&ctx->mut);
    return 0;
}

int
hio_bus_i2c_read(hio_bus_i2c_t *ctx,
                 const hio_bus_i2c_xfer_t *xfer)
{
    hio_bus_i2c_acquire(ctx);

    if (ctx->drv->read != NULL) {
        if (ctx->drv->read(ctx->drv_ctx, xfer) < 0) {
            hio_log_wrn("Call `ctx->drv->read` failed [%p]", ctx);

            hio_bus_i2c_release(ctx);
            return -1;
        }
    }

    hio_bus_i2c_release(ctx);
    return 0;
}

int
hio_bus_i2c_write(hio_bus_i2c_t *ctx,
                  const hio_bus_i2c_xfer_t *xfer)
{
    hio_bus_i2c_acquire(ctx);

    if (ctx->drv->write != NULL) {
        if (ctx->drv->write(ctx->drv_ctx, xfer) < 0) {
            hio_log_wrn("Call `ctx->drv->write` failed [%p]", ctx);

            hio_bus_i2c_release(ctx);
            return -1;
        }
    }

    hio_bus_i2c_release(ctx);
    return 0;
}

int
hio_bus_i2c_mem_read(hio_bus_i2c_t *ctx,
                     const hio_bus_i2c_mem_xfer_t *xfer)
{
    hio_bus_i2c_acquire(ctx);

    if (ctx->drv->mem_read != NULL) {
        if (ctx->drv->mem_read(ctx->drv_ctx, xfer) < 0) {
            hio_log_wrn("Call `ctx->drv->mem_read` failed [%p]", ctx);

            hio_bus_i2c_release(ctx);
            return -1;
        }
    }

    hio_bus_i2c_release(ctx);
    return 0;
}

int
hio_bus_i2c_mem_write(hio_bus_i2c_t *ctx,
                      const hio_bus_i2c_mem_xfer_t *xfer)
{
    hio_bus_i2c_acquire(ctx);

    if (ctx->drv->mem_write != NULL) {
        if (ctx->drv->mem_write(ctx->drv_ctx, xfer) < 0) {
            hio_log_wrn("Call `ctx->drv->mem_write` failed [%p]", ctx);

            hio_bus_i2c_release(ctx);
            return -1;
        }
    }

    hio_bus_i2c_release(ctx);
    return 0;
}

int
hio_bus_i2c_mem_read_8b(hio_bus_i2c_t *ctx,
                        uint8_t dev_addr, uint32_t mem_addr, uint8_t *data)
{
    uint8_t buf[1];

    hio_bus_i2c_mem_xfer_t xfer = {
        .dev_addr = dev_addr,
        .mem_addr = mem_addr,
        .buf = buf,
        .len = sizeof(buf)
    };

    if (hio_bus_i2c_mem_read(ctx, &xfer) < 0) {
        hio_log_wrn("Call `hio_bus_i2c_mem_read` failed [%p]", ctx);
        return -1;
    }

    *data = buf[0];
    return 0;
}

int
hio_bus_i2c_mem_read_16b(hio_bus_i2c_t *ctx,
                         uint8_t dev_addr, uint32_t mem_addr, uint16_t *data)
{
    uint8_t buf[2];

    hio_bus_i2c_mem_xfer_t xfer = {
        .dev_addr = dev_addr,
        .mem_addr = mem_addr,
        .buf = buf,
        .len = sizeof(buf)
    };

    if (hio_bus_i2c_mem_read(ctx, &xfer) < 0) {
        hio_log_wrn("Call `hio_bus_i2c_mem_read` failed [%p]", ctx);
        return -1;
    }

    *data = buf[0] << 8 | buf[1];
    return 0;
}

int
hio_bus_i2c_mem_write_8b(hio_bus_i2c_t *ctx,
                         uint8_t dev_addr, uint32_t mem_addr, uint8_t data)
{
    uint8_t buf[1];

    buf[0] = data;

    hio_bus_i2c_mem_xfer_t xfer = {
        .dev_addr = dev_addr,
        .mem_addr = mem_addr,
        .buf = buf,
        .len = sizeof(buf)
    };

    if (hio_bus_i2c_mem_write(ctx, &xfer) < 0) {
        hio_log_wrn("Call `hio_bus_i2c_mem_write` failed [%p]", ctx);
        return -1;
    }

    return 0;
}

int
hio_bus_i2c_mem_write_16b(hio_bus_i2c_t *ctx,
                          uint8_t dev_addr, uint32_t mem_addr, uint16_t data)
{
    uint8_t buf[2];

    buf[0] = data >> 8;
    buf[1] = data;

    hio_bus_i2c_mem_xfer_t xfer = {
        .dev_addr = dev_addr,
        .mem_addr = mem_addr,
        .buf = buf,
        .len = sizeof(buf)
    };

    if (hio_bus_i2c_mem_write(ctx, &xfer) < 0) {
        hio_log_wrn("Call `hio_bus_i2c_mem_write` failed [%p]", ctx);
        return -1;
    }

    return 0;
}
