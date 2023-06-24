/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_w1.h>
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

LOG_MODULE_REGISTER(ctr_soil_sensor, CONFIG_CTR_SOIL_SENSOR_LOG_LEVEL);

#define TMP112_I2C_ADDR	 0x48
#define TMP112_CONV_TIME K_MSEC(40)

#define ZSSC3123_I2C_ADDR  0x28
#define ZSSC3123_CONV_TIME K_MSEC(5)

struct sensor {
	uint64_t serial_number;
	const struct device *dev;
};

static K_MUTEX_DEFINE(m_lock);

static struct ctr_w1 m_w1;

static struct sensor m_sensors[] = {
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_0))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_1))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_2))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_3))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_4))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_5))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_6))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_7))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_8))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_soil_sensor_9))},
};

static int m_count;

static int tmp112_init(const struct device *dev)
{
	int ret;

	uint8_t write_buf[3];

	write_buf[0] = 0x01;
	write_buf[1] = 0x01;
	write_buf[2] = 0x80;

	ret = ds28e17_i2c_write(dev, TMP112_I2C_ADDR, write_buf, 3);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int tmp112_convert(const struct device *dev)
{
	int ret;

	uint8_t write_buf[2];

	write_buf[0] = 0x01;
	write_buf[1] = 0x81;

	ret = ds28e17_i2c_write(dev, TMP112_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int tmp112_read(const struct device *dev, float *temperature)
{
	int ret;

	uint8_t write_buf[1];
	uint8_t read_buf[2];

	write_buf[0] = 0x01;

	ret = ds28e17_i2c_write_read(dev, TMP112_I2C_ADDR, write_buf, 1, read_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	if ((read_buf[0] & 0x81) != 0x81) {
		LOG_ERR("Conversion not done");
		return -EBUSY;
	}

	write_buf[0] = 0x00;

	ret = ds28e17_i2c_write_read(dev, TMP112_I2C_ADDR, write_buf, 1, read_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	int16_t reg = sys_get_be16(&read_buf[0]);

	*temperature = (reg >> 4) * 0.0625f;

	return 0;
}

static int zssc3123_convert(const struct device *dev)
{
	int ret;

	uint8_t write_buf[1];

	write_buf[0] = 0;

	ret = ds28e17_i2c_write(dev, ZSSC3123_I2C_ADDR, write_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int zssc3123_read(const struct device *dev, int *moisture)
{
	int ret;

	uint8_t read_buf[2];

	ret = ds28e17_i2c_read(dev, ZSSC3123_I2C_ADDR, read_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_read` failed: %d", ret);
		return ret;
	}

	uint16_t reg = sys_get_be16(&read_buf[0]);

	uint8_t status = reg >> 14;

	if (status) {
		LOG_ERR("Data not valid");
		return -EIO;
	}

	*moisture = reg & BIT_MASK(14);

	return 0;
}

static int scan_callback(struct w1_rom rom, void *user_data)
{
	int ret;

	if (rom.family != 0x19) {
		return 0;
	}

	uint64_t serial_number = sys_get_le48(rom.serial);

	if (m_count >= ARRAY_SIZE(m_sensors)) {
		LOG_WRN("No more space for additional device: %llu", serial_number);
		return 0;
	}

	if (!device_is_ready(m_sensors[m_count].dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	struct w1_slave_config config = {.rom = rom};
	ret = ds28e17_set_w1_config(m_sensors[m_count].dev, config);
	if (ret) {
		LOG_ERR("Call `ds28e17_set_w1_config` failed: %d", ret);
		return ret;
	}

	ret = ds28e17_write_config(m_sensors[m_count].dev, DS28E17_I2C_SPEED_100_KHZ);
	if (ret) {
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);
		return ret;
	}

	if (zssc3123_convert(m_sensors[m_count].dev)) {
		LOG_DBG("Skipping serial number: %llu", serial_number);
		return 0;
	}

	m_sensors[m_count++].serial_number = serial_number;

	LOG_DBG("Registered serial number: %llu", serial_number);

	return 0;
}

int ctr_soil_sensor_scan(void)
{
	int ret;
	int res = 0;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_lock);
		return -ENODEV;
	}

	ret = ctr_w1_acquire(&m_w1, dev);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		res = ret;
		goto error;
	}

	m_count = 0;

	ret = ctr_w1_scan(&m_w1, dev, scan_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Call `ctr_w1_scan` failed: %d", ret);
		res = ret;
		goto error;
	}

error:
	ret = ctr_w1_release(&m_w1, dev);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		res = res ? res : ret;
	}

	k_mutex_unlock(&m_lock);

	return res;
}

int ctr_soil_sensor_get_count(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	int count = m_count;
	k_mutex_unlock(&m_lock);

	return count;
}

int ctr_soil_sensor_read(int index, uint64_t *serial_number, float *temperature, int *moisture)
{
	int ret;
	int res = 0;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	if (index < 0 || index >= m_count || !m_count) {
		k_mutex_unlock(&m_lock);
		return -ERANGE;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_lock);
		return -ENODEV;
	}

	if (serial_number) {
		*serial_number = m_sensors[index].serial_number;
	}

	ret = ctr_w1_acquire(&m_w1, dev);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		res = ret;
		goto error;
	}

	if (!device_is_ready(m_sensors[index].dev)) {
		LOG_ERR("Device not ready");
		res = -ENODEV;
		goto error;
	}

	ret = ds28e17_write_config(m_sensors[index].dev, DS28E17_I2C_SPEED_100_KHZ);
	if (ret) {
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);
		res = ret;
		goto error;
	}

	if (!res) {
		ret = tmp112_init(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `tmp112_init` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	if (!res) {
		ret = tmp112_convert(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `tmp112_convert` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(TMP112_CONV_TIME);

	if (!res) {
		ret = tmp112_read(m_sensors[index].dev, temperature);
		if (ret) {
			LOG_ERR("Call `tmp112_read` failed: %d", ret);
			res = ret;
			goto error;
		} else if (temperature) {
			LOG_DBG("Temperature: %.2f C", *temperature);
		}
	}

	if (!res) {
		ret = zssc3123_convert(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `zssc3123_convert` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(ZSSC3123_CONV_TIME);

	if (!res) {
		ret = zssc3123_read(m_sensors[index].dev, moisture);
		if (ret) {
			LOG_ERR("Call `zssc3123_read` failed: %d", ret);
			res = ret;
			goto error;
		} else if (moisture) {
			LOG_DBG("Moisture: %d", *moisture);
		}
	}

error:
	ret = ctr_w1_release(&m_w1, dev);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		res = res ? res : ret;
	}

	k_mutex_unlock(&m_lock);

	return res;
}
