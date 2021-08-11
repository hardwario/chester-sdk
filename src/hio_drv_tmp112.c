#include <hio_drv_tmp112.h>
#include <hio_log.h>
#include <hio_sys.h>

// Standard includes
#include <string.h>

HIO_LOG_REGISTER(hio_drv_tmp112, HIO_LOG_LEVEL_DBG);

int
hio_drv_tmp112_init(hio_drv_tmp112_t *ctx,
                    hio_bus_i2c_t *i2c, uint8_t dev_addr)
{
    memset(ctx, 0, sizeof(*ctx));

    ctx->i2c = i2c;
    ctx->dev_addr = dev_addr;

    return 0;
}

static int
init(hio_drv_tmp112_t *ctx)
{
    if (hio_bus_i2c_mem_write_16b(ctx->i2c, ctx->dev_addr, 0x01, 0x0180) < 0) {
        hio_log_err("Call `hio_bus_i2c_mem_write_16b` failed [%p]", ctx);
        return -1;
    }

    return 0;
}

static int
measure(hio_drv_tmp112_t *ctx)
{
    if (hio_bus_i2c_mem_write_8b(ctx->i2c, ctx->dev_addr, 0x01, 0x81) < 0) {
        hio_log_err("Call `hio_bus_i2c_mem_write_8b` failed [%p]", ctx);
        return -1;
    }

    return 0;
}

static int
read(hio_drv_tmp112_t *ctx, uint16_t *data)
{
    uint8_t reg;

    if (hio_bus_i2c_mem_read_8b(ctx->i2c, ctx->dev_addr, 0x01, &reg) < 0) {
        hio_log_err("Call `hio_bus_i2c_mem_read_8b` failed [%p]", ctx);
        return -1;
    }

    if ((reg & 0x81) != 0x81) {
        hio_log_err("Unexpected value (0x%02x) [%p]", reg, ctx);
        return -2;
    }

    if (hio_bus_i2c_mem_read_16b(ctx->i2c, ctx->dev_addr, 0x00, data) < 0) {
        hio_log_err("Call `hio_bus_i2c_mem_read_16b` failed [%p]", ctx);
        return -3;
    }

    return 0;
}

int
hio_drv_tmp112_measure(hio_drv_tmp112_t *ctx, float *t)
{
    if (!ctx->ready) {
        if (init(ctx) < 0) {
            hio_log_err("Call `init` failed [%p]", ctx);
            return -1;
        }

        ctx->ready = true;

        hio_sys_task_sleep(HIO_SYS_MSEC(100));
    }

    if (measure(ctx) < 0) {
        hio_log_err("Call `measure` failed [%p]", ctx);
        ctx->ready = false;
        return -2;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(100));

    uint16_t data;

    if (read(ctx, &data) < 0) {
        hio_log_err("Call `read` failed [%p]", ctx);
        ctx->ready = false;
        return -3;
    }

    int16_t reg_t = (int16_t)data >> 4;
    *t = (float)reg_t / 16.f;

    return 0;
}

int
hio_drv_tmp112_sleep(hio_drv_tmp112_t *ctx)
{
    if (init(ctx) < 0) {
        hio_log_err("Call `init` failed [%p]", ctx);
        return -1;
    }

    return 0;
}
