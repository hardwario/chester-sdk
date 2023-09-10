/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "adc_tla202x_reg.h"

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util_macro.h>

#define DT_DRV_COMPAT ti_tla2024

LOG_MODULE_REGISTER(tla2024, CONFIG_ADC_LOG_LEVEL);

#define CONVERSION_DELAY K_MSEC(10)

struct tla2024_config {
	const struct i2c_dt_spec i2c_spec;
	int data_rate;
};

struct tla2024_data {
	uint16_t reg_config;
};

static inline const struct tla2024_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct tla2024_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int read(const struct device *dev, uint8_t reg, uint16_t *data)
{
	int ret;

	uint8_t data_[2];

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, &reg, sizeof(reg), data_,
				sizeof(data_));
	if (ret) {
		LOG_ERR("Call `i2c_write_read_dt` failed: %d", ret);
		return ret;
	}

	*data = sys_get_be16(data_);

	return 0;
}

static int write(const struct device *dev, uint8_t reg, uint16_t data)
{
	int ret;

	uint8_t data_[3] = {reg};

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	sys_put_be16(data, &data_[1]);

	ret = i2c_write_dt(&get_config(dev)->i2c_spec, data_, sizeof(data_));
	if (ret) {
		LOG_ERR("Call `i2c_write_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

int tla2024_channel_setup(const struct device *dev, const struct adc_channel_cfg *channel_cfg)
{
	if (channel_cfg->channel_id > 3) {
		LOG_ERR("Invalid channel id");
		return -EINVAL;
	}

	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("Invalid gain");
		return -EINVAL;
	}

	if (channel_cfg->reference != ADC_REF_INTERNAL) {
		LOG_ERR("Invalid reference");
		return -EINVAL;
	}

	if (channel_cfg->acquisition_time != ADC_ACQ_TIME_DEFAULT) {
		LOG_ERR("Invalid acquisition time");
		return -EINVAL;
	}

	return 0;
}

int tla2024_read(const struct device *dev, const struct adc_sequence *sequence)
{
	int ret;

	if (sequence->channels > 0b1111) {
		LOG_ERR("Selected channel(s) not supported");
		return -ENOTSUP;
	}

	if (sequence->resolution != 12) {
		LOG_ERR("Selected resolution not supported");
		return -ENOTSUP;
	}

	if (sequence->oversampling) {
		LOG_ERR("Oversampling not supported");
		return -ENOTSUP;
	}

	if (sequence->calibrate) {
		LOG_ERR("Calibration not supported");
		return -ENOTSUP;
	}

	if (sequence->options) {
		if (sequence->options->extra_samplings) {
			LOG_ERR("Extra samplings not supported");
			return -ENOTSUP;
		}
	}

	if (!sequence->buffer) {
		LOG_ERR("No buffer provided");
		return -EINVAL;
	}

	int16_t *buffer = sequence->buffer;

	if (sequence->buffer_size < sizeof(buffer[0])) {
		LOG_ERR("Buffer too small: %u", sequence->buffer_size);
		return -EINVAL;
	}

	const int channel_count = ((sequence->channels >> 0) & 1) +
				  ((sequence->channels >> 1) & 1) +
				  ((sequence->channels >> 2) & 1) + ((sequence->channels >> 3) & 1);

	size_t num_samples = 1;
	enum adc_action action;

	for (int i = 0; i < num_samples; i += (action == ADC_ACTION_REPEAT ? 0 : 1)) {
		action = ADC_ACTION_CONTINUE;

		for (int j = 0, c = 0; j < 4; j++) {
			if (!((sequence->channels >> j) & 1)) {
				continue;
			}

			uint16_t reg_config = get_data(dev)->reg_config;
			reg_config |= REG_CONFIG_OS_MSK;
			reg_config |= (0b100 | j) << REG_CONFIG_MUX_POS;
			ret = write(dev, REG_CONFIG, reg_config);
			if (ret) {
				LOG_ERR("Failed to start single-shot conversion: %d", ret);
				return ret;
			}

			k_sleep(CONVERSION_DELAY);

			ret = read(dev, REG_DATA, &buffer[i * channel_count + c]);
			if (ret) {
				LOG_ERR("Call `read` failed: %d", ret);
				return ret;
			}

			buffer[i * channel_count + c] >>= REG_DATA_POS;

			c++;
		}

		if (sequence->options && sequence->options->callback) {
			action = sequence->options->callback(dev, sequence, i * channel_count);
		}

		if (action == ADC_ACTION_FINISH) {
			break;
		}
	}

	return 0;
}

static int tla2024_init(const struct device *dev)
{
	int ret;

	get_data(dev)->reg_config = REG_CONFIG_DEFAULT;
	get_data(dev)->reg_config |= get_config(dev)->data_rate << REG_CONFIG_DR_POS;
	get_data(dev)->reg_config |= REG_CONFIG_MODE_DEFAULT << REG_CONFIG_MODE_POS;
	get_data(dev)->reg_config |= REG_CONFIG_PGA_DEFAULT << REG_CONFIG_PGA_POS;
	get_data(dev)->reg_config |= REG_CONFIG_MUX_DEFAULT << REG_CONFIG_MUX_POS;

	LOG_DBG("Configuration register: 0x%04x", get_data(dev)->reg_config);

	ret = write(dev, REG_CONFIG, get_data(dev)->reg_config);
	if (ret) {
		LOG_ERR("Device reset failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct adc_driver_api tla2024_driver_api = {
	.channel_setup = tla2024_channel_setup,
	.read = tla2024_read,
	.ref_internal = 4096 /* [-2048, 2047] mV */,
};

#define TLA2024_INIT(n)                                                                            \
	static const struct tla2024_config inst_##n##_config = {                                   \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.data_rate = DT_INST_PROP(n, data_rate),                                           \
	};                                                                                         \
	static struct tla2024_data inst_##n##_data = {};                                           \
	DEVICE_DT_INST_DEFINE(n, &tla2024_init, NULL, &inst_##n##_data, &inst_##n##_config,        \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &tla2024_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TLA2024_INIT)
