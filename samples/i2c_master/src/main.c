/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define TMP112_I2C_ADDR          0x48
#define TMP112_REG_TEMPERATURE   0x00
#define TMP112_REG_CONFIGURATION 0x01

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

static int read(const struct device *dev, uint8_t addr, uint8_t reg, uint16_t *data)
{
	int ret;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = i2c_write_read(dev, addr, &reg, 1, data, 2);
	if (ret) {
		LOG_ERR("Call `i2c_write_read` failed: %d", ret);
		return ret;
	}

	*data = sys_be16_to_cpu(*data);

	LOG_DBG("Register: 0x%02x Data: 0x%04x", reg, *data);

	return 0;
}

static int write(const struct device *dev, uint8_t addr, uint8_t reg, uint16_t data)
{
	int ret;

	LOG_DBG("Register: 0x%02x Data: 0x%04x", reg, data);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	uint8_t buf[3] = {reg};
	sys_put_be16(data, &buf[1]);

	ret = i2c_write(dev, buf, sizeof(buf), addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

int main(void)
{
	int ret;

	for (;;) {
		LOG_INF("Alive");

		ret = write(dev, TMP112_I2C_ADDR, TMP112_REG_CONFIGURATION, 0x0180);
		if (ret) {
			LOG_ERR("Call `write` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(50));

		ret = write(dev, TMP112_I2C_ADDR, TMP112_REG_CONFIGURATION, 0x8180);
		if (ret) {
			LOG_ERR("Call `write` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_MSEC(50));

		uint16_t reg_configuration;
		ret = read(dev, TMP112_I2C_ADDR, TMP112_REG_CONFIGURATION, &reg_configuration);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			k_oops();
		}

		if ((reg_configuration & 0x8100) != 0x8100) {
			LOG_ERR("Conversion is not done");
			k_oops();
		}

		uint16_t reg_temperature;
		ret = read(dev, TMP112_I2C_ADDR, TMP112_REG_TEMPERATURE, &reg_temperature);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			k_oops();
		}

		float temperature = ((int16_t)reg_temperature >> 4) / 16.f;

		LOG_INF("Temperature: %.2f C", temperature);

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
