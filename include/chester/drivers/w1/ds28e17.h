/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRIVERS_W1_DS28E17_H_
#define CHESTER_INCLUDE_DRIVERS_W1_DS28E17_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/w1.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ds28e17_i2c_speed {
	DS28E17_I2C_SPEED_100_KHZ = 0,
	DS28E17_I2C_SPEED_400_KHZ = 1,
	DS28E17_I2C_SPEED_900_KHZ = 2,
};

typedef int (*ds28e17_api_set_w1_config)(const struct device *dev, struct w1_slave_config config);
typedef int (*ds28e17_api_i2c_write)(const struct device *dev, uint8_t dev_addr,
				     const uint8_t *write_buf, size_t write_len);
typedef int (*ds28e17_api_i2c_read)(const struct device *dev, uint8_t dev_addr, uint8_t *read_buf,
				    size_t read_len);
typedef int (*ds28e17_api_i2c_write_read)(const struct device *dev, uint8_t dev_addr,
					  const uint8_t *write_buf, size_t write_len,
					  uint8_t *read_buf, size_t read_len);
typedef int (*ds28e17_api_write_config)(const struct device *dev, enum ds28e17_i2c_speed i2c_speed);
typedef int (*ds28e17_api_enable_sleep)(const struct device *dev);

struct ds28e17_driver_api {
	ds28e17_api_set_w1_config set_w1_config;
	ds28e17_api_i2c_write i2c_write;
	ds28e17_api_i2c_read i2c_read;
	ds28e17_api_i2c_write_read i2c_write_read;
	ds28e17_api_write_config write_config;
	ds28e17_api_enable_sleep enable_sleep;
};

static inline int ds28e17_set_w1_config(const struct device *dev, struct w1_slave_config config)
{
	const struct ds28e17_driver_api *api = (const struct ds28e17_driver_api *)dev->api;

	return api->set_w1_config(dev, config);
}

static inline int ds28e17_i2c_write(const struct device *dev, uint8_t dev_addr,
				    const uint8_t *write_buf, size_t write_len)
{
	const struct ds28e17_driver_api *api = (const struct ds28e17_driver_api *)dev->api;

	return api->i2c_write(dev, dev_addr, write_buf, write_len);
}

static inline int ds28e17_i2c_read(const struct device *dev, uint8_t dev_addr, uint8_t *read_buf,
				   size_t read_len)
{
	const struct ds28e17_driver_api *api = (const struct ds28e17_driver_api *)dev->api;

	return api->i2c_read(dev, dev_addr, read_buf, read_len);
}

static inline int ds28e17_i2c_write_read(const struct device *dev, uint8_t dev_addr,
					 const uint8_t *write_buf, size_t write_len,
					 uint8_t *read_buf, size_t read_len)
{
	const struct ds28e17_driver_api *api = (const struct ds28e17_driver_api *)dev->api;

	return api->i2c_write_read(dev, dev_addr, write_buf, write_len, read_buf, read_len);
}

static inline int ds28e17_write_config(const struct device *dev, enum ds28e17_i2c_speed i2c_speed)
{
	const struct ds28e17_driver_api *api = (const struct ds28e17_driver_api *)dev->api;

	return api->write_config(dev, i2c_speed);
}

static inline int ds28e17_enable_sleep(const struct device *dev)
{
	const struct ds28e17_driver_api *api = (const struct ds28e17_driver_api *)dev->api;

	return api->enable_sleep(dev);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRIVERS_W1_DS28E17_H_ */
