#include <ctr_bus_i2c.h>

/* Zephyr includes */
#include <logging/log.h>

/* Standard includes */
#include <string.h>

LOG_MODULE_REGISTER(hio_bus_i2c, LOG_LEVEL_INF);

int hio_bus_i2c_init(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_driver *drv, void *drv_ctx)
{
	int ret;

	memset(ctx, 0, sizeof(*ctx));

	if (drv == NULL) {
		LOG_ERR("Driver cannot be NULL [%p]", ctx);
		return -EINVAL;
	}

	ctx->drv = drv;
	ctx->drv_ctx = drv_ctx;

	k_mutex_init(&ctx->mut);

	if (ctx->drv->init != NULL) {
		ret = ctx->drv->init(ctx->drv_ctx);

		if (ret < 0) {
			LOG_ERR("Call `ctx->drv->init` failed [%p]: %d", ctx, ret);
			return ret;
		}
	}

	return 0;
}

int hio_bus_i2c_acquire(struct hio_bus_i2c *ctx)
{
	int ret;

	k_mutex_lock(&ctx->mut, K_FOREVER);

	if ((ctx->acq_cnt)++ == 0) {
		if (ctx->drv->enable != NULL) {
			LOG_DBG("Bus enable [%p]", ctx);

			ret = ctx->drv->enable(ctx->drv_ctx);

			if (ret < 0) {
				LOG_ERR("Call `ctx->drv->enable` failed [%p]: %d", ctx, ret);

				k_mutex_unlock(&ctx->mut);
				return ret;
			}
		}
	}

	return 0;
}

int hio_bus_i2c_release(struct hio_bus_i2c *ctx)
{
	int ret;

	if (ctx->acq_cnt == 0) {
		LOG_ERR("Bus not acquired [%p]", ctx);
		k_oops();
		return -ENOLCK;
	}

	if (--(ctx->acq_cnt) == 0) {
		if (ctx->drv->disable != NULL) {
			LOG_DBG("Bus disable [%p]", ctx);

			ret = ctx->drv->disable(ctx->drv_ctx);

			if (ret < 0) {
				LOG_ERR("Call `ctx->drv->disable` failed [%p]: %d", ctx, ret);

				k_mutex_unlock(&ctx->mut);
				return ret;
			}
		}
	}

	k_mutex_unlock(&ctx->mut);
	return 0;
}

int hio_bus_i2c_read(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_xfer *xfer)
{
	int ret;

	hio_bus_i2c_acquire(ctx);

	if (ctx->drv->read != NULL) {
		ret = ctx->drv->read(ctx->drv_ctx, xfer);

		if (ret < 0) {
			LOG_WRN("Call `ctx->drv->read` failed [%p]: %d", ctx, ret);

			hio_bus_i2c_release(ctx);
			return ret;
		}
	}

	hio_bus_i2c_release(ctx);
	return 0;
}

int hio_bus_i2c_write(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_xfer *xfer)
{
	int ret;

	hio_bus_i2c_acquire(ctx);

	if (ctx->drv->write != NULL) {
		ret = ctx->drv->write(ctx->drv_ctx, xfer);

		if (ret < 0) {
			LOG_WRN("Call `ctx->drv->write` failed [%p]: %d", ctx, ret);

			hio_bus_i2c_release(ctx);
			return ret;
		}
	}

	hio_bus_i2c_release(ctx);
	return 0;
}

int hio_bus_i2c_mem_read(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_mem_xfer *xfer)
{
	int ret;

	hio_bus_i2c_acquire(ctx);

	if (ctx->drv->mem_read != NULL) {
		ret = ctx->drv->mem_read(ctx->drv_ctx, xfer);

		if (ret < 0) {
			LOG_WRN("Call `ctx->drv->mem_read` failed [%p]: %d", ctx, ret);

			hio_bus_i2c_release(ctx);
			return ret;
		}
	}

	hio_bus_i2c_release(ctx);
	return 0;
}

int hio_bus_i2c_mem_write(struct hio_bus_i2c *ctx, const struct hio_bus_i2c_mem_xfer *xfer)
{
	int ret;

	hio_bus_i2c_acquire(ctx);

	if (ctx->drv->mem_write != NULL) {
		ret = ctx->drv->mem_write(ctx->drv_ctx, xfer);

		if (ret < 0) {
			LOG_WRN("Call `ctx->drv->mem_write` failed [%p]: %d", ctx, ret);

			hio_bus_i2c_release(ctx);
			return ret;
		}
	}

	hio_bus_i2c_release(ctx);
	return 0;
}

int hio_bus_i2c_mem_read_8b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                            uint8_t *data)
{
	int ret;
	uint8_t buf[1];

	struct hio_bus_i2c_mem_xfer xfer = {
		.dev_addr = dev_addr, .mem_addr = mem_addr, .buf = buf, .len = sizeof(buf)
	};

	ret = hio_bus_i2c_mem_read(ctx, &xfer);

	if (ret < 0) {
		LOG_WRN("Call `hio_bus_i2c_mem_read` failed [%p]: %d", ctx, ret);
		return ret;
	}

	*data = buf[0];
	return 0;
}

int hio_bus_i2c_mem_read_16b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                             uint16_t *data)
{
	int ret;
	uint8_t buf[2];

	struct hio_bus_i2c_mem_xfer xfer = {
		.dev_addr = dev_addr, .mem_addr = mem_addr, .buf = buf, .len = sizeof(buf)
	};

	ret = hio_bus_i2c_mem_read(ctx, &xfer);

	if (ret < 0) {
		LOG_WRN("Call `hio_bus_i2c_mem_read` failed [%p]: %d", ctx, ret);
		return ret;
	}

	*data = buf[0] << 8 | buf[1];
	return 0;
}

int hio_bus_i2c_mem_write_8b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                             uint8_t data)
{
	int ret;
	uint8_t buf[1];

	buf[0] = data;

	struct hio_bus_i2c_mem_xfer xfer = {
		.dev_addr = dev_addr, .mem_addr = mem_addr, .buf = buf, .len = sizeof(buf)
	};

	ret = hio_bus_i2c_mem_write(ctx, &xfer);

	if (ret < 0) {
		LOG_WRN("Call `hio_bus_i2c_mem_write` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

int hio_bus_i2c_mem_write_16b(struct hio_bus_i2c *ctx, uint8_t dev_addr, uint32_t mem_addr,
                              uint16_t data)
{
	int ret;
	uint8_t buf[2];

	buf[0] = data >> 8;
	buf[1] = data;

	struct hio_bus_i2c_mem_xfer xfer = {
		.dev_addr = dev_addr, .mem_addr = mem_addr, .buf = buf, .len = sizeof(buf)
	};

	ret = hio_bus_i2c_mem_write(ctx, &xfer);

	if (ret < 0) {
		LOG_WRN("Call `hio_bus_i2c_mem_write` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}
