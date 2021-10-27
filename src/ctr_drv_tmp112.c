#include <ctr_drv_tmp112.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <string.h>

LOG_MODULE_REGISTER(ctr_drv_tmp112, LOG_LEVEL_DBG);

int ctr_drv_tmp112_init(struct ctr_drv_tmp112 *ctx, struct ctr_bus_i2c *i2c, uint8_t dev_addr)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->i2c = i2c;
	ctx->dev_addr = dev_addr;

	return 0;
}

static int init(struct ctr_drv_tmp112 *ctx)
{
	int ret;

	ret = ctr_bus_i2c_mem_write_16b(ctx->i2c, ctx->dev_addr, 0x01, 0x0180);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bus_i2c_mem_write_16b` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

static int measure(struct ctr_drv_tmp112 *ctx)
{
	int ret;

	ret = ctr_bus_i2c_mem_write_8b(ctx->i2c, ctx->dev_addr, 0x01, 0x81);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bus_i2c_mem_write_8b` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

static int read(struct ctr_drv_tmp112 *ctx, uint16_t *data)
{
	int ret;
	uint8_t reg;

	ret = ctr_bus_i2c_mem_read_8b(ctx->i2c, ctx->dev_addr, 0x01, &reg);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bus_i2c_mem_read_8b` failed [%p]: %d", ctx, ret);
		return ret;
	}

	if ((reg & 0x81) != 0x81) {
		LOG_ERR("Unexpected value (0x%02x) [%p]", reg, ctx);
		return -EIO;
	}

	ret = ctr_bus_i2c_mem_read_16b(ctx->i2c, ctx->dev_addr, 0x00, data);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bus_i2c_mem_read_16b` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

int ctr_drv_tmp112_measure(struct ctr_drv_tmp112 *ctx, float *t)
{
	int ret;

	if (!ctx->ready) {
		ret = init(ctx);

		if (ret < 0) {
			LOG_ERR("Call `init` failed [%p]: %d", ctx, ret);
			return ret;
		}

		ctx->ready = true;

		k_sleep(K_MSEC(100));
	}

	ret = measure(ctx);

	if (ret < 0) {
		LOG_ERR("Call `measure` failed [%p]: %d", ctx, ret);
		ctx->ready = false;
		return ret;
	}

	k_sleep(K_MSEC(100));

	uint16_t data;

	ret = read(ctx, &data);

	if (ret < 0) {
		LOG_ERR("Call `read` failed [%p]: %d", ctx, ret);
		ctx->ready = false;
		return ret;
	}

	int16_t reg_t = (int16_t)data >> 4;
	*t = (float)reg_t / 16.f;

	return 0;
}

int ctr_drv_tmp112_sleep(struct ctr_drv_tmp112 *ctx)
{
	int ret;

	ret = init(ctx);

	if (ret < 0) {
		LOG_ERR("Call `init` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}
