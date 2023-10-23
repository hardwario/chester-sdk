/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_w1.h>
#include <chester/ctr_weight_probe.h>
#include <chester/drivers/w1/ds28e17.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_weight_probe, CONFIG_CTR_WEIGHT_PROBE_LOG_LEVEL);

#define ADS122C04_I2C_ADDR  0x40
#define ADS122C04_IDLE_TIME K_MSEC(100)

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

struct sensor {
	uint64_t serial_number;
	const struct device *dev;
};

static K_MUTEX_DEFINE(m_lock);

static struct ctr_w1 m_w1;

static struct sensor m_sensors[] = {
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_0))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_1))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_2))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_3))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_4))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_5))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_6))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_7))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_8))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_weight_probe_9))},
};

static int m_count;

static int ads122c04_send_cmd(const struct device *dev, uint8_t cmd)
{
	int ret;

	uint8_t write_buf[1];

	write_buf[0] = cmd;

	ret = ds28e17_i2c_write(dev, ADS122C04_I2C_ADDR, write_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Command: 0x%02x", cmd);

	return 0;
}

static int ads122c04_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
	int ret;

	LOG_DBG("Register: 0x%02x Value: 0x%02x", reg, val);

	uint8_t write_buf[2];

	write_buf[0] = CMD_WREG | (reg & 0x03) << 2;
	write_buf[1] = val;

	ret = ds28e17_i2c_write(dev, ADS122C04_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ads122c04_read_reg(const struct device *dev, uint8_t reg, uint8_t *val)
{
	int ret;

	uint8_t write_buf[1];

	write_buf[0] = CMD_RREG | (reg & 0x03) << 2;

	ret = ds28e17_i2c_write_read(dev, ADS122C04_I2C_ADDR, write_buf, 1, val, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Register: 0x%02x Value: 0x%02x", reg, *val);

	return 0;
}

static int ads122c04_read_data(const struct device *dev, uint8_t data[3])
{
	int ret;

	uint8_t write_buf[1];

	write_buf[0] = CMD_RDATA;

	ret = ds28e17_i2c_write_read(dev, ADS122C04_I2C_ADDR, write_buf, 1, data, 3);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Data: 0x%02x%02x%02x", data[0], data[1], data[2]);

	return 0;
}

static int ads122c04_init(const struct device *dev)
{
	int ret;

	ret = ads122c04_send_cmd(dev, CMD_RESET);
	if (ret) {
		LOG_ERR("Call `ads122c04_send_cmd` (CMD_RESET) failed: %d", ret);
		return ret;
	}

	uint8_t reg_config_0 = DEF_CONFIG_0;
	uint8_t reg_config_1 = DEF_CONFIG_1;
	uint8_t reg_config_2 = DEF_CONFIG_2;
	uint8_t reg_config_3 = DEF_CONFIG_3;

	reg_config_0 |= 7 << POS_CONFIG_0_GAIN;
	reg_config_1 |= 4 << POS_CONFIG_1_DR;
	reg_config_1 |= 1 << POS_CONFIG_1_VREF;
	reg_config_2 |= 5 << POS_CONFIG_2_IDAC;
	reg_config_3 |= 3 << POS_CONFIG_3_I1MUX;

	ret = ads122c04_write_reg(dev, REG_CONFIG_0, reg_config_0);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` (REG_CONFIG_0) failed: %d", ret);
		return ret;
	}

	ret = ads122c04_write_reg(dev, REG_CONFIG_1, reg_config_1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` (REG_CONFIG_1) failed: %d", ret);
		return ret;
	}

	ret = ads122c04_write_reg(dev, REG_CONFIG_2, reg_config_2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` (REG_CONFIG_2) failed: %d", ret);
		return ret;
	}

	ret = ads122c04_write_reg(dev, REG_CONFIG_3, reg_config_3);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` (REG_CONFIG_3) failed: %d", ret);
		return ret;
	}

	ret = ads122c04_send_cmd(dev, CMD_POWERDOWN);
	if (ret) {
		LOG_ERR("Call `ads122c04_send_cmd` (CMD_POWERDOWN) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ads122c04_read(const struct device *dev, uint8_t data[3])
{
	int ret;

	ret = ads122c04_send_cmd(dev, CMD_START_SYNC);
	if (ret) {
		LOG_ERR("Call `ads122c04_send_cmd` (CMD_START_SYNC) failed: %d", ret);
		goto error;
	}

	k_sleep(ADS122C04_IDLE_TIME);

	ret = ads122c04_send_cmd(dev, CMD_START_SYNC);
	if (ret) {
		LOG_ERR("Call `ads122c04_send_cmd` (CMD_START_SYNC) failed: %d", ret);
		goto error;
	}

	int retries = 0;

	for (;;) {
		uint8_t val;
		ret = ads122c04_read_reg(dev, REG_CONFIG_2, &val);
		if (ret) {
			LOG_ERR("Call `ads122c04_read_reg` (REG_CONFIG_2) failed: %d", ret);
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

	ret = ads122c04_read_data(dev, data);
	if (ret) {
		LOG_ERR("Call `ads122c04_read_data` failed: %d", ret);
		goto error;
	}

	ret = ads122c04_send_cmd(dev, CMD_POWERDOWN);
	if (ret) {
		LOG_ERR("Call `ads122c04_send_cmd` (CMD_POWERDOWN) failed: %d", ret);
		goto error;
	}

	return 0;

error:
	ads122c04_send_cmd(dev, CMD_POWERDOWN);

	return ret;
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

	if (ads122c04_send_cmd(m_sensors[m_count].dev, CMD_POWERDOWN)) {
		LOG_DBG("Skipping serial number: %llu", serial_number);
		return 0;
	}

	m_sensors[m_count++].serial_number = serial_number;

	LOG_DBG("Registered serial number: %llu", serial_number);

	return 0;
}

int ctr_weight_probe_scan(void)
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

int ctr_weight_probe_get_count(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	int count = m_count;
	k_mutex_unlock(&m_lock);

	return count;
}

int ctr_weight_probe_read(int index, uint64_t *serial_number, int32_t *result)
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
		ret = ads122c04_init(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `ads122c04_init` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	if (!res) {
		uint8_t data[3];
		ret = ads122c04_read(m_sensors[index].dev, data);
		if (ret) {
			LOG_ERR("Call `ads122c04_read` failed: %d", ret);
			res = ret;
			goto error;
		}

		*result = sys_get_be24(data);
		*result <<= 8;
		*result >>= 8;
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
