#include <hio_drv_ads122c04.h>

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_drv_ads122c04, LOG_LEVEL_DBG);

enum cmd {
	CMD_RESET = 0x06,
	CMD_START_SYNC = 0x08,
	CMD_POWERDOWN = 0x02,
	CMD_RDATA = 0x10,
	CMD_RREG = 0x20,
	CMD_WREG = 0x40,
};

static int send_cmd(struct hio_drv_ads122c04 *ctx, enum cmd cmd)
{
	int ret;

	struct hio_bus_i2c_mem_xfer xfer = {
		.dev_addr = ctx->dev_addr, .mem_addr = (uint8_t)cmd, .buf = NULL, .len = 0
	};

	ret = hio_bus_i2c_mem_write(ctx->i2c, &xfer);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_drv_ads122c04_init(struct hio_drv_ads122c04 *ctx, struct hio_bus_i2c *i2c, uint8_t dev_addr)
{
	ctx->i2c = i2c;
	ctx->dev_addr = dev_addr;

	return 0;
}

int hio_drv_ads122c04_reset(struct hio_drv_ads122c04 *ctx)
{
	int ret;

	ret = send_cmd(ctx, CMD_RESET);

	if (ret < 0) {
		LOG_ERR("Call `send_cmd` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_drv_ads122c04_start_sync(struct hio_drv_ads122c04 *ctx)
{
	int ret;

	ret = send_cmd(ctx, CMD_START_SYNC);

	if (ret < 0) {
		LOG_ERR("Call `send_cmd` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_drv_ads122c04_powerdown(struct hio_drv_ads122c04 *ctx)
{
	int ret;

	ret = send_cmd(ctx, CMD_POWERDOWN);

	if (ret < 0) {
		LOG_ERR("Call `send_cmd` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_drv_ads122c04_read_reg(struct hio_drv_ads122c04 *ctx, uint8_t addr, uint8_t *data)
{
	int ret;

	ret = hio_bus_i2c_mem_read_8b(ctx->i2c, ctx->dev_addr, CMD_RREG | addr << 2, data);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_read_8b` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_drv_ads122c04_write_reg(struct hio_drv_ads122c04 *ctx, uint8_t addr, uint8_t data)
{
	int ret;

	ret = hio_bus_i2c_mem_write_8b(ctx->i2c, ctx->dev_addr, CMD_WREG | addr << 2, data);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_write_8b` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_drv_ads122c04_read_data(struct hio_drv_ads122c04 *ctx, int32_t *data)
{
	int ret;

	uint8_t buf[3];

	struct hio_bus_i2c_mem_xfer xfer = {
		.dev_addr = ctx->dev_addr, .mem_addr = (uint8_t)CMD_RDATA, .buf = buf, .len = 3
	};

	ret = hio_bus_i2c_mem_read(ctx->i2c, &xfer);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_read` failed: %d", ret);
		return ret;
	}

	*data = buf[0] << 24 | buf[1] << 16 | buf[2] << 8;
	*data >>= 8;

	return 0;
}
