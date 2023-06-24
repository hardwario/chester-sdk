/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/w1/ds28e17.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DT_DRV_COMPAT maxim_ds28e17

LOG_MODULE_REGISTER(ds28e17, CONFIG_W1_LOG_LEVEL);

#define DEV_CMD_WRITE_DATA_STOP	     0x4b
#define DEV_CMD_READ_DATA_STOP	     0x87
#define DEV_CMD_WRITE_READ_DATA_STOP 0x2d
#define DEV_CMD_WRITE_CONFIG	     0xd2
#define DEV_CMD_ENABLE_SLEEP	     0x1e

#define POLL_BUSY_MAX_ATTEMPTS 100
#define POLL_BUSY_DELAY	       K_MSEC(10)

struct ds28e17_config {
	const struct device *bus;
	uint8_t family;
};

struct ds28e17_data {
	const struct device *dev;
	struct w1_slave_config config;
};

static inline const struct ds28e17_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ds28e17_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ds28e17_set_w1_config_(const struct device *dev, struct w1_slave_config config)
{
	if (config.rom.family != get_config(dev)->family) {
		return -ERANGE;
	}

	get_data(dev)->config = config;

	return 0;
}

static int poll_busy(const struct device *dev)
{
	int ret;

	for (int i = 0; i < POLL_BUSY_MAX_ATTEMPTS; i++) {
		ret = w1_read_bit(get_config(dev)->bus);
		if (ret < 0) {
			LOG_ERR("Call `w1_read_bit` failed: %d", ret);
			return ret;
		} else if (!ret) {
			return 0;
		}

		k_sleep(POLL_BUSY_DELAY);
	}

	return -ETIMEDOUT;
}

static int ds28e17_i2c_write_(const struct device *dev, uint8_t dev_addr, const uint8_t *write_buf,
			      size_t write_len)
{
	int ret;

	if (write_len < 1 || write_len > 255) {
		return -EINVAL;
	}

	ret = w1_lock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_lock_bus` failed: %d", ret);
		return ret;
	}

	ret = w1_reset_select(get_config(dev)->bus, &get_data(dev)->config);
	if (ret) {
		LOG_ERR("Call `w1_reset_select` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t *buf = k_malloc(3 + write_len + 2);
	if (!buf) {
		LOG_ERR("Call `k_malloc` failed");
		w1_unlock_bus(get_config(dev)->bus);
		return -ENOMEM;
	}

	buf[0] = DEV_CMD_WRITE_DATA_STOP;
	buf[1] = dev_addr << 1;
	buf[2] = write_len;

	memcpy(&buf[3], write_buf, write_len);

	uint16_t crc16 = w1_crc16(W1_CRC16_SEED, buf, 3 + write_len);
	sys_put_le16(~crc16, &buf[3 + write_len]);

	ret = w1_write_block(get_config(dev)->bus, buf, 3 + write_len + 2);
	if (ret) {
		LOG_ERR("Call `w1_write_block` failed: %d", ret);
		k_free(buf);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	k_free(buf);

	ret = poll_busy(dev);
	if (ret) {
		LOG_ERR("Call `poll_busy` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t status;
	ret = w1_read_block(get_config(dev)->bus, &status, 1);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	if (status != 0) {
		LOG_ERR("Error in status: 0x%02x", status);
		w1_unlock_bus(get_config(dev)->bus);
		return -EIO;
	}

	uint8_t write_status;
	ret = w1_read_block(get_config(dev)->bus, &write_status, 1);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	if (write_status != 0) {
		LOG_ERR("Error in write status: 0x%02x", write_status);
		w1_unlock_bus(get_config(dev)->bus);
		return -EIO;
	}

	ret = w1_unlock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ds28e17_i2c_read_(const struct device *dev, uint8_t dev_addr, uint8_t *read_buf,
			     size_t read_len)
{
	int ret;

	if (read_len < 1 || read_len > 255) {
		return -EINVAL;
	}

	ret = w1_lock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_lock_bus` failed: %d", ret);
		return ret;
	}

	ret = w1_reset_select(get_config(dev)->bus, &get_data(dev)->config);
	if (ret) {
		LOG_ERR("Call `w1_reset_select` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t *buf = k_malloc(3 + 2);
	if (!buf) {
		LOG_ERR("Call `k_malloc` failed");
		w1_unlock_bus(get_config(dev)->bus);
		return -ENOMEM;
	}

	buf[0] = DEV_CMD_READ_DATA_STOP;
	buf[1] = dev_addr << 1 | BIT(0);
	buf[2] = read_len;

	uint16_t crc16 = w1_crc16(W1_CRC16_SEED, buf, 3);
	sys_put_le16(~crc16, &buf[3]);

	ret = w1_write_block(get_config(dev)->bus, buf, 3 + 2);
	if (ret) {
		LOG_ERR("Call `w1_write_block` failed: %d", ret);
		k_free(buf);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	k_free(buf);

	ret = poll_busy(dev);
	if (ret) {
		LOG_ERR("Call `poll_busy` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t status;
	ret = w1_read_block(get_config(dev)->bus, &status, 1);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	if (status != 0) {
		LOG_ERR("Error in status: 0x%02x", status);
		w1_unlock_bus(get_config(dev)->bus);
		return -EIO;
	}

	ret = w1_read_block(get_config(dev)->bus, read_buf, read_len);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	ret = w1_unlock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ds28e17_i2c_write_read_(const struct device *dev, uint8_t dev_addr,
				   const uint8_t *write_buf, size_t write_len, uint8_t *read_buf,
				   size_t read_len)
{
	int ret;

	if (write_len < 1 || write_len > 255 || read_len < 1 || read_len > 255) {
		return -EINVAL;
	}

	ret = w1_lock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_lock_bus` failed: %d", ret);
		return ret;
	}

	ret = w1_reset_select(get_config(dev)->bus, &get_data(dev)->config);
	if (ret) {
		LOG_ERR("Call `w1_reset_select` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t *buf = k_malloc(3 + write_len + 3);
	if (!buf) {
		LOG_ERR("Call `k_malloc` failed");
		w1_unlock_bus(get_config(dev)->bus);
		return -ENOMEM;
	}

	buf[0] = DEV_CMD_WRITE_READ_DATA_STOP;
	buf[1] = dev_addr << 1;
	buf[2] = write_len;

	memcpy(&buf[3], write_buf, write_len);

	buf[3 + write_len] = read_len;

	uint16_t crc16 = w1_crc16(W1_CRC16_SEED, buf, 3 + write_len + 1);
	sys_put_le16(~crc16, &buf[3 + write_len + 1]);

	ret = w1_write_block(get_config(dev)->bus, buf, 3 + write_len + 3);
	if (ret) {
		LOG_ERR("Call `w1_write_block` failed: %d", ret);
		k_free(buf);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	k_free(buf);

	ret = poll_busy(dev);
	if (ret) {
		LOG_ERR("Call `poll_busy` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t status;
	ret = w1_read_block(get_config(dev)->bus, &status, 1);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	if (status != 0) {
		LOG_ERR("Error in status: 0x%02x", status);
		w1_unlock_bus(get_config(dev)->bus);
		return -EIO;
	}

	uint8_t write_status;
	ret = w1_read_block(get_config(dev)->bus, &write_status, 1);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	if (write_status != 0) {
		LOG_ERR("Error in write status: 0x%02x", write_status);
		w1_unlock_bus(get_config(dev)->bus);
		return -EIO;
	}

	ret = w1_read_block(get_config(dev)->bus, read_buf, read_len);
	if (ret) {
		LOG_ERR("Call `w1_read_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	ret = w1_unlock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ds28e17_write_config_(const struct device *dev, enum ds28e17_i2c_speed i2c_speed)
{
	int ret;

	if (i2c_speed != DS28E17_I2C_SPEED_100_KHZ && i2c_speed != DS28E17_I2C_SPEED_400_KHZ &&
	    i2c_speed != DS28E17_I2C_SPEED_900_KHZ) {
		return -EINVAL;
	}

	ret = w1_lock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_lock_bus` failed: %d", ret);
		return ret;
	}

	ret = w1_reset_select(get_config(dev)->bus, &get_data(dev)->config);
	if (ret) {
		LOG_ERR("Call `w1_reset_select` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t buf[2];

	buf[0] = DEV_CMD_WRITE_CONFIG;
	buf[1] = i2c_speed;

	ret = w1_write_block(get_config(dev)->bus, buf, 2);
	if (ret) {
		LOG_ERR("Call `w1_write_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	ret = w1_unlock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ds28e17_enable_sleep_(const struct device *dev)
{
	int ret;

	ret = w1_lock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_lock_bus` failed: %d", ret);
		return ret;
	}

	ret = w1_reset_select(get_config(dev)->bus, &get_data(dev)->config);
	if (ret) {
		LOG_ERR("Call `w1_reset_select` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	uint8_t buf[1];

	buf[0] = DEV_CMD_ENABLE_SLEEP;

	ret = w1_write_block(get_config(dev)->bus, buf, 1);
	if (ret) {
		LOG_ERR("Call `w1_write_block` failed: %d", ret);
		w1_unlock_bus(get_config(dev)->bus);
		return ret;
	}

	ret = w1_unlock_bus(get_config(dev)->bus);
	if (ret) {
		LOG_ERR("Call `w1_unlock_bus` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ds28e17_driver_api ds28e17_driver_api = {
	.set_w1_config = ds28e17_set_w1_config_,
	.i2c_read = ds28e17_i2c_read_,
	.i2c_write = ds28e17_i2c_write_,
	.i2c_write_read = ds28e17_i2c_write_read_,
	.write_config = ds28e17_write_config_,
	.enable_sleep = ds28e17_enable_sleep_,
};

static int ds28e17_init(const struct device *dev)
{
	if (!device_is_ready(get_config(dev)->bus)) {
		LOG_ERR("Bus not ready");
		return -ENODEV;
	}

	return 0;
}

#define DS28E17_INIT(n)                                                                            \
	static const struct ds28e17_config inst_##n##_config = {                                   \
		.bus = DEVICE_DT_GET(DT_INST_BUS(n)),                                              \
		.family = DT_INST_PROP(n, family_code),                                            \
	};                                                                                         \
	static struct ds28e17_data inst_##n##_data = {                                             \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ds28e17_init, NULL, &inst_##n##_data, &inst_##n##_config,         \
			      POST_KERNEL, CONFIG_W1_INIT_PRIORITY, &ds28e17_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DS28E17_INIT)
