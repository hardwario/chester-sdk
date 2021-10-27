#include <ctr_drv_sht30.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <string.h>

LOG_MODULE_REGISTER(hio_drv_sht30, LOG_LEVEL_DBG);

int hio_drv_sht30_init(struct hio_drv_sht30 *ctx, struct hio_bus_i2c *i2c, uint8_t dev_addr)
{
	memset(ctx, 0, sizeof(*ctx));

	ctx->i2c = i2c;
	ctx->dev_addr = dev_addr;

	return 0;
}

static int reset(struct hio_drv_sht30 *ctx)
{
	int ret;

	uint8_t buf[] = { 0x30, 0xa2 };

	struct hio_bus_i2c_xfer xfer = { .dev_addr = ctx->dev_addr,
		                         .buf = buf,
		                         .len = sizeof(buf) };

	ret = hio_bus_i2c_write(ctx->i2c, &xfer);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_write` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

static int measure(struct hio_drv_sht30 *ctx)
{
	int ret;

	uint8_t buf[] = { 0x24, 0x00 };

	struct hio_bus_i2c_xfer xfer = { .dev_addr = ctx->dev_addr,
		                         .buf = buf,
		                         .len = sizeof(buf) };

	ret = hio_bus_i2c_write(ctx->i2c, &xfer);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_write` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

static int read(struct hio_drv_sht30 *ctx, uint8_t *buf)
{
	int ret;

	struct hio_bus_i2c_xfer xfer = { .dev_addr = ctx->dev_addr, .buf = buf, .len = 6 };

	ret = hio_bus_i2c_read(ctx->i2c, &xfer);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_read` failed [%p]: %d", ctx, ret);
		return ret;
	}

	return 0;
}

static uint8_t calc_crc(uint8_t *data, size_t len)
{
	uint16_t crc = 0xff;

	for (size_t i = 0; i < len; i++) {
		crc ^= data[i];

		for (size_t j = 0; j < 8; j++) {
			if ((crc & 0x80) != 0) {
				crc <<= 1;
				crc ^= 0x131;

			} else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

int hio_drv_sht30_measure(struct hio_drv_sht30 *ctx, float *t, float *rh)
{
	int ret;

	if (!ctx->ready) {
		ret = reset(ctx);

		if (ret < 0) {
			LOG_ERR("Call `reset` failed [%p]: %d", ctx, ret);
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

	uint8_t buf[6];

	ret = read(ctx, buf);

	if (ret < 0) {
		LOG_ERR("Call `read` failed [%p]: %d", ctx, ret);
		ctx->ready = false;
		return ret;
	}

	if (calc_crc(&buf[0], 2) != buf[2] || calc_crc(&buf[3], 2) != buf[5]) {
		LOG_ERR("CRC mismatch [%p]", ctx);
		return -EIO;
	}

	uint16_t reg_t = buf[0] << 8 | buf[1];
	*t = -45.f + 175.f * (float)reg_t / 65535.f;

	uint16_t reg_rh = buf[3] << 8 | buf[4];
	*rh = 100.f * (float)reg_rh / 65535.f;

	return 0;
}
