/*
* Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_pm3006s.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT cubic_pm3006s

LOG_MODULE_REGISTER(ctr_pm3006s, CONFIG_CTR_PM3006S_LOG_LEVEL);

struct ctr_pm3006s_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
};

struct ctr_pm3006s_data {
	const struct device *dev;
};

static inline const struct ctr_pm3006s_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_pm3006s_data *get_data(const struct device *dev)
{
	return dev->data;
}


static int ctr_pm3006s_open_measurement_(const struct device *dev)
{
	int ret;

	uint8_t reg = 0x16;
	uint8_t buf[8] = {reg, 0x16, 0x07, 0x02, 0x00, 0x00, 0x00, 0x10};

	ret = i2c_write(get_config(dev)->i2c_dev, buf, sizeof(buf), get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_pm3006s_close_measurement_(const struct device *dev)
{
	int ret;

	uint8_t reg = 0x16;
	uint8_t buf[8] = {reg, 0x16, 0x07, 0x01, 0x00, 0x00, 0x00, 0x13};

	ret = i2c_write(get_config(dev)->i2c_dev, buf, sizeof(buf), get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}


static int ctr_pm3006s_read_measurement_(const struct device *dev, struct ctr_pm3006s_measurement *measurement)
{
	int ret;

	uint8_t reg = 0x16;
	uint8_t buf[32];

	ret = i2c_read(get_config(dev)->i2c_dev, buf, sizeof(buf), get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_read` failed: %d", ret);
		return ret;
	}

	measurement->sensor_status = buf[2];
	measurement->tsp = sys_get_be16(&buf[5]);
	measurement->pm1 = sys_get_be16(&buf[7]);
	measurement->pm2_5 = sys_get_be16(&buf[9]);
	measurement->pm10 = sys_get_be16(&buf[11]);
	measurement->q0_3um = sys_get_be24(&buf[13]);
	measurement->q0_5um = sys_get_be24(&buf[16]);
	measurement->q1_0um = sys_get_be24(&buf[19]);
	measurement->q2_5um = sys_get_be24(&buf[22]);
	measurement->q5_0um = sys_get_be24(&buf[25]);
	measurement->q10um = sys_get_be24(&buf[28]);

	return 0;
}

static int ctr_pm3006s_init(const struct device *dev)
{
	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	return 0;
}

static const struct ctr_pm3006s_driver_api ctr_pm3006s_driver_api = {
	.open_measurement = ctr_pm3006s_open_measurement_,
	.close_measurement = ctr_pm3006s_close_measurement_,
	.read_measurement = ctr_pm3006s_read_measurement_,
};

#define CTR_PM3006S_INIT(n) 								           \
	static struct ctr_pm3006s_data inst_##n##_data = {                                         \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	}; 										           \
	static const struct ctr_pm3006s_config inst_##n##_config = { 			           \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                          \
		.i2c_addr = DT_INST_REG_ADDR(n) 					           \
	}; 										           \
	DEVICE_DT_DEFINE(DT_NODELABEL(ctr_pm3006s), ctr_pm3006s_init, NULL, &inst_##n##_data,      \
		 &inst_##n##_config, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,                        \
		 &ctr_pm3006s_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_PM3006S_INIT)
