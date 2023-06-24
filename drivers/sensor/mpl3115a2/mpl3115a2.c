/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(mpl3115a2, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT nxp_mpl3115a2

#define MPL3115A2_REG_STATUS	  0x00
#define MPL3115A2_REG_OUT_P_MSB	  0x01
#define MPL3115A2_REG_WHO_AM_I	  0x0c
#define MPL3115A2_REG_PT_DATA_CFG 0x13
#define MPL3115A2_REG_CTRL_REG1	  0x26

#define MPL3115A2_VALUE_WHO_AM_I 0xc4

#define MPL3115A2_RESET_DELAY_MSEC 1000
#define MPL3115A2_CONV_TIME_MSEC   512

struct mpl3115a2_data {
	float altitude;
	float pressure;
	float temperature;
	int64_t reset_time;
};

struct mpl3115a2_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
};

static inline const struct mpl3115a2_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct mpl3115a2_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int read(const struct device *dev, uint8_t reg, uint8_t *data, size_t num_read)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = i2c_write_read(get_config(dev)->i2c_dev, get_config(dev)->i2c_addr, &reg, 1, data,
			     num_read);
	if (ret) {
		LOG_ERR("Call `i2c_write_read` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Register: 0x%02x Length: 0x%02x", reg, num_read);
	LOG_HEXDUMP_DBG(data, num_read, "Data");

	return 0;
}

static int write(const struct device *dev, uint8_t reg, uint8_t data)
{
	int ret;

	LOG_DBG("Register: 0x%02x Data: 0x%02x", reg, data);

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	uint8_t buf[2] = {reg, data};

	ret = i2c_write(get_config(dev)->i2c_dev, buf, sizeof(buf), get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int sample_altitude(const struct device *dev)
{
	int ret;

	/* Set altimeter mode, set oversampling 2^7 = 128 */
	ret = write(dev, MPL3115A2_REG_CTRL_REG1, 0xb8);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	/* Activate altitude, pressure and temperature event flag generator */
	ret = write(dev, MPL3115A2_REG_PT_DATA_CFG, 0x07);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	/* Initiate altitute measurement */
	ret = write(dev, MPL3115A2_REG_CTRL_REG1, 0xba);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	/* Measurement delay */
	k_sleep(K_MSEC(MPL3115A2_CONV_TIME_MSEC));

	/* Check STATUS register */
	uint8_t reg_status;
	ret = read(dev, MPL3115A2_REG_STATUS, &reg_status, sizeof(reg_status));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	/* Check if data is ready */
	if (reg_status != 0x0e) {
		LOG_ERR("Data not ready");
		return -EACCES;
	}

	/* Read 5 bytes, 3 for altitude and 2 for temperature */
	uint8_t buffer[5];
	ret = read(dev, MPL3115A2_REG_OUT_P_MSB, buffer, sizeof(buffer));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	int32_t a = sys_get_be24(&buffer[0]) << 8;
	get_data(dev)->altitude = (a >> 12) / 16.f;
	get_data(dev)->temperature = (int8_t)buffer[3] + (buffer[4] >> 4) / 16.f;

	return 0;
}

static int sample_pressure(const struct device *dev)
{
	int ret;

	/* Set pressure mode, set oversampling 2^7 = 128 */
	ret = write(dev, MPL3115A2_REG_CTRL_REG1, 0x38);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	/* Activate altitude, pressure and temperature event flag generator */
	ret = write(dev, MPL3115A2_REG_PT_DATA_CFG, 0x07);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	/* Initiate pressure measurement */
	ret = write(dev, MPL3115A2_REG_CTRL_REG1, 0x3a);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	/* Measurement delay */
	k_sleep(K_MSEC(MPL3115A2_CONV_TIME_MSEC));

	/* Check STATUS register */
	uint8_t reg_status;
	ret = read(dev, MPL3115A2_REG_STATUS, &reg_status, sizeof(reg_status));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	/* Check if data is ready */
	if (reg_status != 0x0e) {
		LOG_ERR("Data not ready");
		return -EACCES;
	}

	/* Read 5 bytes, 3 for altitude and 2 for temperature */
	uint8_t buffer[5];
	ret = read(dev, MPL3115A2_REG_OUT_P_MSB, buffer, sizeof(buffer));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint32_t p = sys_get_be24(&buffer[0]) << 8;
	get_data(dev)->pressure = (p >> 12) / 4.f;
	get_data(dev)->temperature = (int8_t)buffer[3] + (buffer[4] >> 4) / 16.f;

	return 0;
}

static int mpl3115a2_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	int ret;

	/* Check if delay after the reset and initialization passed */
	int64_t delta = k_uptime_get() - get_data(dev)->reset_time;
	if (delta < MPL3115A2_RESET_DELAY_MSEC) {
		k_sleep(K_MSEC(MPL3115A2_RESET_DELAY_MSEC - delta));
	}

	/* Sensor measures temperature together with altitude or pressure measurement */
	if (chan == SENSOR_CHAN_ALTITUDE || chan == SENSOR_CHAN_AMBIENT_TEMP ||
	    chan == SENSOR_CHAN_ALL) {
		ret = sample_altitude(dev);
		if (ret) {
			LOG_ERR("Call `sample_altitude` failed: %d", ret);
			return ret;
		}
	}

	if (chan == SENSOR_CHAN_PRESS || chan == SENSOR_CHAN_ALL) {
		ret = sample_pressure(dev);
		if (ret) {
			LOG_ERR("Call `sample_pressure` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int mpl3115a2_channel_get(const struct device *dev, enum sensor_channel chan,
				 struct sensor_value *val)
{
	if (chan == SENSOR_CHAN_ALTITUDE) {
		sensor_value_from_double(val, get_data(dev)->altitude);
	} else if (chan == SENSOR_CHAN_PRESS) {
		sensor_value_from_double(val, get_data(dev)->pressure);
	} else if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
		sensor_value_from_double(val, get_data(dev)->temperature);
	} else {
		return -ENOTSUP;
	}

	return 0;
}

static int mpl3115a2_init(const struct device *dev)
{
	int ret;

	/* Check WHO_AM_I */
	uint8_t who_am_i;
	ret = read(dev, MPL3115A2_REG_WHO_AM_I, &who_am_i, sizeof(who_am_i));
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}
	if (who_am_i != MPL3115A2_VALUE_WHO_AM_I) {
		return -EIO;
	}

	/* Reset sensor */
	write(dev, MPL3115A2_REG_CTRL_REG1, 0x04);
	/* Do not check return value, sensor does not answer after reset */

	get_data(dev)->reset_time = k_uptime_get();

	return 0;
}

static const struct sensor_driver_api mpl3115a2_driver_api = {
	.sample_fetch = mpl3115a2_sample_fetch,
	.channel_get = mpl3115a2_channel_get,
};

#define MPL3115_A2_INIT(n)                                                                         \
	static const struct mpl3115a2_config inst_##n##_config = {                                 \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                          \
		.i2c_addr = DT_INST_REG_ADDR(n),                                                   \
	};                                                                                         \
	static struct mpl3115a2_data inst_##n##_data;                                              \
	SENSOR_DEVICE_DT_INST_DEFINE(n, mpl3115a2_init, NULL, &inst_##n##_data,                    \
				     &inst_##n##_config, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,    \
				     &mpl3115a2_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MPL3115_A2_INIT)
