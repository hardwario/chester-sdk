/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <stddef.h>
#include <stdint.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT ti_ads122c04

LOG_MODULE_REGISTER(ads122c04, CONFIG_ADC_LOG_LEVEL);

#define CMD_RESET      0x06
#define CMD_START_SYNC 0x08
#define CMD_POWERDOWN  0x02
#define CMD_RDATA      0x10
#define CMD_RREG       0x20
#define CMD_WREG       0x40

#define REG_CONFIG_0 0x00
#define REG_CONFIG_1 0x01
#define REG_CONFIG_2 0x02
#define REG_CONFIG_3 0x03

#define DEF_CONFIG_0 0x00
#define DEF_CONFIG_1 0x00
#define DEF_CONFIG_2 0x00
#define DEF_CONFIG_3 0x00

#define POS_CONFIG_0_PGA_BYPASS 0
#define POS_CONFIG_0_GAIN	1
#define POS_CONFIG_0_MUX	4
#define POS_CONFIG_1_TS		0
#define POS_CONFIG_1_VREF	1
#define POS_CONFIG_1_CM		3
#define POS_CONFIG_1_MODE	4
#define POS_CONFIG_1_DR		5
#define POS_CONFIG_2_IDAC	0
#define POS_CONFIG_2_BCS	3
#define POS_CONFIG_2_CRC	4
#define POS_CONFIG_2_DCNT	6
#define POS_CONFIG_2_DRDY	7
#define POS_CONFIG_3_I2MUX	2
#define POS_CONFIG_3_I1MUX	5

#define MSK_CONFIG_0_MUX	(BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define MSK_CONFIG_0_PGA_BYPASS BIT(0)
#define MSK_CONFIG_0_GAIN	(BIT(1) | BIT(2) | BIT(3))
#define MSK_CONFIG_1_TS		BIT(0)
#define MSK_CONFIG_1_VREF	(BIT(1) | BIT(2))
#define MSK_CONFIG_1_CM		BIT(3)
#define MSK_CONFIG_1_MODE	BIT(4)
#define MSK_CONFIG_1_DR		(BIT(5) | BIT(6) | BIT(7))
#define MSK_CONFIG_2_IDAC	(BIT(0) | BIT(1) | BIT(2))
#define MSK_CONFIG_2_BCS	BIT(3)
#define MSK_CONFIG_2_CRC	(BIT(4) | BIT(5))
#define MSK_CONFIG_2_DCNT	BIT(6)
#define MSK_CONFIG_2_DRDY	BIT(7)
#define MSK_CONFIG_3_I2MUX	(BIT(2) | BIT(3) | BIT(4))
#define MSK_CONFIG_3_I1MUX	(BIT(5) | BIT(6) | BIT(7))

#define READ_MAX_RETRIES    100
#define READ_RETRY_DELAY_MS 10

struct ads122c04_config {
	const struct i2c_dt_spec i2c_spec;
	int gain;
	int mux;
	int vref;
	int idac;
	int i1mux;
	int i2mux;
};

struct ads122c04_data {
	struct k_sem lock;
	uint8_t reg_config_0;
	uint8_t reg_config_1;
	uint8_t reg_config_2;
	uint8_t reg_config_3;
};

static inline const struct ads122c04_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ads122c04_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int send_cmd(const struct device *dev, uint8_t cmd)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_write_dt(&get_config(dev)->i2c_spec, &cmd, 1);
	if (ret) {
		LOG_ERR("Call `i2c_write_dt` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Command: 0x%02x", cmd);

	return 0;
}

static int read_data(const struct device *dev, uint8_t data[3])
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	uint8_t cmd = CMD_RDATA;

	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, &cmd, 1, data, 3);
	if (ret) {
		LOG_ERR("Call `i2c_write_read_dt` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Data: 0x%02x%02x%02x", data[0], data[1], data[2]);

	return 0;
}

static int read_reg(const struct device *dev, uint8_t reg, uint8_t *val)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	reg = CMD_RREG | (reg & 0x03) << 2;

	ret = i2c_reg_read_byte_dt(&get_config(dev)->i2c_spec, reg, val);
	if (ret) {
		LOG_ERR("Call `i2c_reg_read_byte_dt` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Register: 0x%02x Value: 0x%02x", reg, *val);

	return 0;
}

static int write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	int ret;

	LOG_DBG("Register: 0x%02x Value: 0x%02x", reg, val);

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	reg = CMD_WREG | (reg & 0x03) << 2;

	ret = i2c_reg_write_byte_dt(&get_config(dev)->i2c_spec, reg, val);
	if (ret) {
		LOG_ERR("Call `i2c_reg_write_byte_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ads122c04_channel_setup(const struct device *dev,
				   const struct adc_channel_cfg *channel_cfg)
{
	return 0;
}

static int ads122c04_read(const struct device *dev, const struct adc_sequence *sequence)
{
	int ret;

	if (sequence->buffer_size < sizeof(int32_t)) {
		return -ENOSPC;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&get_data(dev)->lock, K_FOREVER);

	ret = send_cmd(dev, CMD_START_SYNC);
	if (ret) {
		LOG_ERR("Call `send_cmd` (CMD_START_SYNC) failed: %d", ret);
		goto error;
	}

	int retries = 0;

	for (;;) {
		uint8_t val;
		ret = read_reg(dev, REG_CONFIG_2, &val);
		if (ret) {
			LOG_ERR("Call `read_reg` (REG_CONFIG_2) failed: %d", ret);
			goto error;
		}

		if (val & MSK_CONFIG_2_DRDY) {
			break;
		}

		if (++retries >= READ_MAX_RETRIES) {
			LOG_ERR("Reached maximum DRDY poll attempts");
			ret = -EIO;
			goto error;
		}

		k_msleep(READ_RETRY_DELAY_MS);
	}

	uint8_t data[3];
	ret = read_data(dev, data);
	if (ret) {
		LOG_ERR("Call `read_data` failed: %d", ret);
		goto error;
	}

	ret = send_cmd(dev, CMD_POWERDOWN);
	if (ret) {
		LOG_ERR("Call `send_cmd` (CMD_POWERDOWN) failed: %d", ret);
		goto error;
	}

	int32_t *result = sequence->buffer;

	*result = (uint32_t)data[0] << 16 | (uint32_t)data[1] << 8 | (uint32_t)data[2];
	*result <<= 8;
	*result >>= 8;

	k_sem_give(&get_data(dev)->lock);

	return 0;

error:
	send_cmd(dev, CMD_POWERDOWN);

	k_sem_give(&get_data(dev)->lock);

	return ret;
}

static int ads122c04_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	ret = send_cmd(dev, CMD_RESET);
	if (ret) {
		LOG_ERR("Call `send_cmd` (CMD_RESET) failed: %d", ret);
		return ret;
	}

	get_data(dev)->reg_config_0 = DEF_CONFIG_0;
	get_data(dev)->reg_config_0 |= get_config(dev)->gain << POS_CONFIG_0_GAIN;
	get_data(dev)->reg_config_0 |= get_config(dev)->mux << POS_CONFIG_0_MUX;
	get_data(dev)->reg_config_1 = DEF_CONFIG_1;
	get_data(dev)->reg_config_1 |= get_config(dev)->vref << POS_CONFIG_1_VREF;
	get_data(dev)->reg_config_2 = DEF_CONFIG_2;
	get_data(dev)->reg_config_2 |= get_config(dev)->idac << POS_CONFIG_2_IDAC;
	get_data(dev)->reg_config_3 = DEF_CONFIG_3;
	get_data(dev)->reg_config_3 |= get_config(dev)->i1mux << POS_CONFIG_3_I1MUX;
	get_data(dev)->reg_config_3 |= get_config(dev)->i2mux << POS_CONFIG_3_I2MUX;

	ret = write_reg(dev, REG_CONFIG_0, get_data(dev)->reg_config_0);
	if (ret) {
		LOG_ERR("Call `write_reg` (REG_CONFIG_0) failed: %d", ret);
		return ret;
	}

	ret = write_reg(dev, REG_CONFIG_1, get_data(dev)->reg_config_1);
	if (ret) {
		LOG_ERR("Call `write_reg` (REG_CONFIG_1) failed: %d", ret);
		return ret;
	}

	ret = write_reg(dev, REG_CONFIG_2, get_data(dev)->reg_config_2);
	if (ret) {
		LOG_ERR("Call `write_reg` (REG_CONFIG_2) failed: %d", ret);
		return ret;
	}

	ret = write_reg(dev, REG_CONFIG_3, get_data(dev)->reg_config_3);
	if (ret) {
		LOG_ERR("Call `write_reg` (REG_CONFIG_3) failed: %d", ret);
		return ret;
	}

	k_sem_give(&get_data(dev)->lock);

	return 0;
}

static const struct adc_driver_api ads122c04_driver_api = {
	.channel_setup = ads122c04_channel_setup,
	.read = ads122c04_read,
	.ref_internal = 4097 /* [-2048, 2048] mV */,
};

#define ADS122C04_INIT(n)                                                                          \
	static const struct ads122c04_config inst_##n##_config = {                                 \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.gain = DT_INST_PROP(n, gain),                                                     \
		.mux = DT_INST_PROP(n, mux),                                                       \
		.vref = DT_INST_PROP(n, vref),                                                     \
		.idac = DT_INST_PROP(n, idac),                                                     \
		.i1mux = DT_INST_PROP(n, i1mux),                                                   \
		.i2mux = DT_INST_PROP(n, i2mux),                                                   \
	};                                                                                         \
	static struct ads122c04_data inst_##n##_data = {                                           \
		.lock = Z_SEM_INITIALIZER(inst_##n##_data.lock, 0, 1),                             \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ads122c04_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ads122c04_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ADS122C04_INIT)
