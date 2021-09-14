#include <hio_bsp_i2c.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/i2c.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_bsp_i2c, LOG_LEVEL_DBG);

static const struct device *dev;

static int
init(void *ctx)
{
    dev = (struct device *)ctx;

    uint32_t dev_config = I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER;

	if (i2c_configure(dev, dev_config) < 0) {
        LOG_ERR("Call `i2c_configure` failed [%p]", ctx);
        return -1;
    }

    // TODO Uncomment
    #if 0
    if (i2c_recover_bus(dev) < 0) {
        LOG_ERR("Call `i2c_recover_bus` failed [%p]", ctx);
        return -2;
    }
    #endif

    return 0;
}

static int
enable(void *ctx)
{
    return 0;
}

static int
disable(void *ctx)
{
    return 0;
}

static int
read(void *ctx, const hio_bus_i2c_xfer_t *xfer)
{
    struct i2c_msg msgs[1];

    msgs[0].buf = xfer->buf;
    msgs[0].len = xfer->len;
    msgs[0].flags = I2C_MSG_READ | I2C_MSG_STOP;

    if (i2c_transfer(dev, msgs, ARRAY_SIZE(msgs), xfer->dev_addr) < 0) {
        LOG_WRN("Call `i2c_transfer` failed [%p]", ctx);
        return -1;
    }

    return 0;
}

static int
write(void *ctx, const hio_bus_i2c_xfer_t *xfer)
{
    struct i2c_msg msgs[1];

    msgs[0].buf = xfer->buf;
    msgs[0].len = xfer->len;
    msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

    if (i2c_transfer(dev, msgs, ARRAY_SIZE(msgs), xfer->dev_addr) < 0) {
        LOG_WRN("Call `i2c_transfer` failed [%p]", ctx);
        return -1;
    }

    return 0;
}

static int
mem_read(void *ctx, const hio_bus_i2c_mem_xfer_t *xfer)
{
    struct i2c_msg msgs[2];

    msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_RESTART;

    msgs[1].buf = xfer->buf;
    msgs[1].len = xfer->len;
    msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

    if ((xfer->mem_addr & HIO_BUS_I2C_MEM_ADDR_16B) != 0) {

        uint8_t buf[2] = {
            xfer->mem_addr >> 8,
            xfer->mem_addr
        };

        msgs[0].buf = buf;
        msgs[0].len = sizeof(buf);

        if (i2c_transfer(dev, msgs, ARRAY_SIZE(msgs),
                         xfer->dev_addr) < 0) {
            LOG_WRN("Call `i2c_transfer` failed [%p]", ctx);
            return -1;
        }

    } else {

        uint8_t buf[1] = {
            xfer->mem_addr
        };

        msgs[0].buf = buf;
        msgs[0].len = sizeof(buf);

        if (i2c_transfer(dev, msgs, ARRAY_SIZE(msgs),
                         xfer->dev_addr) < 0) {
            LOG_WRN("Call `i2c_transfer` failed [%p]", ctx);
            return -2;
        }
    }

    return 0;
}

static int
mem_write(void *ctx, const hio_bus_i2c_mem_xfer_t *xfer)
{
    if (xfer->len > 64) {
        LOG_ERR("Write request too large [%p]", ctx);
        return -1;
    }

    if ((xfer->mem_addr & HIO_BUS_I2C_MEM_ADDR_16B) != 0) {
        uint8_t buf[2 + 64];

        buf[0] = xfer->mem_addr >> 8;
        buf[1] = xfer->mem_addr;

        memcpy(&buf[2], xfer->buf, xfer->len);

        struct i2c_msg msgs[1];

        msgs[0].buf = buf;
        msgs[0].len = 2 + xfer->len;
        msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        if (i2c_transfer(dev, msgs, ARRAY_SIZE(msgs),
                         xfer->dev_addr) < 0) {
            LOG_WRN("Call `i2c_transfer` failed [%p]", ctx);
            return -2;
        }

    } else {
        uint8_t buf[1 + 64];

        buf[0] = xfer->mem_addr;

        memcpy(&buf[1], xfer->buf, xfer->len);

        struct i2c_msg msgs[1];

        msgs[0].buf = buf;
        msgs[0].len = 1 + xfer->len;
        msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

        if (i2c_transfer(dev, msgs, ARRAY_SIZE(msgs),
                         xfer->dev_addr) < 0) {
            LOG_WRN("Call `i2c_transfer` failed [%p]", ctx);
            return -3;
        }
    }

    return 0;
}

const hio_bus_i2c_driver_t *
hio_bsp_i2c_get_driver(void)
{
    static hio_bus_i2c_driver_t driver = {
        .init = init,
        .enable = enable,
        .disable = disable,
        .read = read,
        .write = write,
        .mem_read = mem_read,
        .mem_write = mem_write
    };

    return &driver;
}
