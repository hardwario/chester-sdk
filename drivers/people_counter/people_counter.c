/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "people_counter_reg.h"

/* CHESTER includes */
#include <chester/drivers/people_counter.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT adastra_people_counter

LOG_MODULE_REGISTER(people_counter, CONFIG_PEOPLE_COUNTER_LOG_LEVEL);

struct people_counter_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
};

struct people_counter_data {
	const struct device *dev;
};

static inline const struct people_counter_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct people_counter_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int read(const struct device *dev, uint8_t reg, uint8_t *data)
{
	int ret;

	ret = i2c_write_read(get_config(dev)->i2c_dev, get_config(dev)->i2c_addr, &reg, 1, data, 1);
	if (ret) {
		LOG_ERR("Call `i2c_write_read` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int write(const struct device *dev, uint8_t reg, uint8_t data)
{
	int ret;

	uint8_t buf[2] = {reg, data};

	ret = i2c_write(get_config(dev)->i2c_dev, buf, 2, get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int people_counter_read_measurement_(const struct device *dev,
					    struct people_counter_measurement *measurement)
{
	int ret;

	uint8_t reg = 0;
	uint8_t buf[18];
	ret = i2c_write_read(get_config(dev)->i2c_dev, get_config(dev)->i2c_addr, &reg, 1, buf,
			     sizeof(buf));
	if (ret) {
		LOG_ERR("Call `i2c_write_read` failed: %d", ret);
		return ret;
	}

	measurement->motion_counter = sys_get_le16(&buf[0]);
	measurement->pass_counter_adult = sys_get_le16(&buf[2]);
	measurement->pass_counter_child = sys_get_le16(&buf[4]);
	measurement->stay_counter_adult = sys_get_le16(&buf[6]);
	measurement->stay_counter_child = sys_get_le16(&buf[8]);
	measurement->total_time_adult = sys_get_le16(&buf[10]);
	measurement->total_time_child = sys_get_le16(&buf[12]);
	measurement->consumed_energy = sys_get_le32(&buf[14]);

	return 0;
}

static int people_counter_get_power_off_delay_(const struct device *dev, int *value)
{
	int ret;

	uint8_t data;
	ret = read(dev, 0x18, &data);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*value = data;

	return 0;
}

static int people_counter_get_stay_timeout_(const struct device *dev, int *value)
{
	int ret;

	uint8_t data;
	ret = read(dev, 0x19, &data);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*value = data;

	return 0;
}

static int people_counter_get_adult_border_(const struct device *dev, int *value)
{
	int ret;

	uint8_t data;
	ret = read(dev, 0x1a, &data);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*value = data;

	return 0;
}

static int people_counter_set_power_off_delay_(const struct device *dev, int value)
{
	int ret;

	if (value > 255) {
		return -EINVAL;
	}

	ret = write(dev, 0x18, value);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int people_counter_set_stay_timeout_(const struct device *dev, int value)
{
	int ret;

	if (value > 255) {
		return -EINVAL;
	}

	ret = write(dev, 0x19, value);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int people_counter_set_adult_border_(const struct device *dev, int value)
{
	int ret;

	if (value > 8) {
		return -EINVAL;
	}

	ret = write(dev, 0x1a, value);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int people_counter_init(const struct device *dev)
{
	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	return 0;
}

static const struct people_counter_driver_api people_counter_driver_api = {
	.read_measurement = people_counter_read_measurement_,
	.get_power_off_delay = people_counter_get_power_off_delay_,
	.get_stay_timeout = people_counter_get_stay_timeout_,
	.get_adult_border = people_counter_get_adult_border_,
	.set_power_off_delay = people_counter_set_power_off_delay_,
	.set_stay_timeout = people_counter_set_stay_timeout_,
	.set_adult_border = people_counter_set_adult_border_,
};

#define PEOPLE_COUNTER_INIT(n)                                                                     \
	static const struct people_counter_config inst_##n##_config = {                            \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                          \
		.i2c_addr = DT_INST_REG_ADDR(n),                                                   \
	};                                                                                         \
	static struct people_counter_data inst_##n##_data = {                                      \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, people_counter_init, NULL, &inst_##n##_data, &inst_##n##_config,  \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &people_counter_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PEOPLE_COUNTER_INIT)
