/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_machine_probe.h>
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
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_machine_probe, CONFIG_CTR_MACHINE_PROBE_LOG_LEVEL);

#define TMP112_I2C_ADDR	 0x48
#define TMP112_INIT_TIME K_MSEC(10)
#define TMP112_CONV_TIME K_MSEC(50)

#define SHT30_I2C_ADDR	0x45
#define SHT30_INIT_TIME K_MSEC(100)
#define SHT30_CONV_TIME K_MSEC(100)

#define OPT3001_I2C_ADDR  0x44
#define OPT3001_INIT_TIME K_MSEC(10)
#define OPT3001_CONV_TIME K_MSEC(2000)

#define SI7210_I2C_ADDR 0x32

#define LIS2DH12_I2C_ADDR 0x19

struct sensor {
	uint64_t serial_number;
	const struct device *dev;
};

static K_MUTEX_DEFINE(m_lock);

static struct ctr_w1 m_w1;

static struct sensor m_sensors[] = {
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_0))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_1))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_2))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_3))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_4))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_5))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_6))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_7))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_8))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ctr_machine_probe_9))},
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
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
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

	if (temperature) {
		*temperature = (sys_get_be16(&read_buf[0]) >> 4) * 0.0625f;
	}

	return 0;
}

static int sht30_init(const struct device *dev)
{
	int ret;

	uint8_t write_buf[2];

	write_buf[0] = 0x30;
	write_buf[1] = 0xa2;

	ret = ds28e17_i2c_write(dev, SHT30_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int sht30_convert(const struct device *dev)
{
	int ret;

	uint8_t write_buf[2];

	write_buf[0] = 0x24;
	write_buf[1] = 0x00;

	ret = ds28e17_i2c_write(dev, SHT30_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int sht30_read(const struct device *dev, float *temperature, float *humidity)
{
	int ret;

	uint8_t read_buf[6];

	ret = ds28e17_i2c_read(dev, SHT30_I2C_ADDR, read_buf, 6);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_read` failed: %d", ret);
		return ret;
	}

	if (temperature) {
		*temperature = -45.f + 175.f * (float)sys_get_be16(&read_buf[0]) / 65535.f;
	}

	if (humidity) {
		*humidity = 100.f * (float)sys_get_be16(&read_buf[3]) / 65535.f;
	}

	return 0;
}

static int opt3001_init(const struct device *dev)
{
	int ret;

	uint8_t write_buf[3];

	write_buf[0] = 0x01;
	write_buf[1] = 0xc8;
	write_buf[2] = 0x10;

	ret = ds28e17_i2c_write(dev, OPT3001_I2C_ADDR, write_buf, 3);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int opt3001_convert(const struct device *dev)
{
	int ret;

	uint8_t write_buf[3];

	write_buf[0] = 0x01;
	write_buf[1] = 0xca;
	write_buf[2] = 0x10;

	ret = ds28e17_i2c_write(dev, OPT3001_I2C_ADDR, write_buf, 3);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int opt3001_read(const struct device *dev, float *illuminance)
{
	int ret;

	uint8_t write_buf[1];
	uint8_t read_buf[2];

	write_buf[0] = 0x01;

	ret = ds28e17_i2c_write_read(dev, OPT3001_I2C_ADDR, write_buf, 1, read_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	if ((read_buf[0] & 0x06) != 0x00 || (read_buf[0] & 0x80) != 0x80) {
		LOG_ERR("Unexpected response");
		return -EIO;
	}

	write_buf[0] = 0x00;

	ret = ds28e17_i2c_write_read(dev, OPT3001_I2C_ADDR, write_buf, 1, read_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	if (illuminance) {
		uint16_t reg = sys_get_be16(&read_buf[0]);
		*illuminance = ((1 << (reg >> 12)) * (reg & 0xfff)) * 0.01f;
	}

	return 0;
}

static int si7210_read(const struct device *dev, float *magnetic_field)
{
	int ret;

	uint8_t write_buf[2];
	uint8_t read_buf[1];

	ret = ds28e17_i2c_read(dev, SI7210_I2C_ADDR, read_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_read` failed: %d", ret);
		return ret;
	}

	write_buf[0] = 0xc4;
	write_buf[1] = 0x04;

	ret = ds28e17_i2c_write(dev, SI7210_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	write_buf[0] = 0xc1;

	ret = ds28e17_i2c_write_read(dev, SI7210_I2C_ADDR, write_buf, 1, read_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	uint8_t reg_dspsigm = read_buf[0];

	write_buf[0] = 0xc2;

	ret = ds28e17_i2c_write_read(dev, SI7210_I2C_ADDR, write_buf, 1, read_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	uint8_t reg_dspsigl = read_buf[0];

	write_buf[0] = 0xc4;
	write_buf[1] = 0x01;

	ret = ds28e17_i2c_write(dev, SI7210_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	if (magnetic_field) {
		*magnetic_field = ((256 * (reg_dspsigm & 0x7f) + reg_dspsigl) - 16384) * 0.00125f;
	}

	return 0;
}

static int lis2dh12_init(const struct device *dev)
{
	int ret;

	uint8_t write_buf[2];

#define WRITE_REG(reg, val)                                                                        \
	do {                                                                                       \
		write_buf[0] = reg;                                                                \
		write_buf[1] = val;                                                                \
		ret = ds28e17_i2c_write(dev, LIS2DH12_I2C_ADDR, write_buf, 2);                     \
		if (ret) {                                                                         \
			LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);                       \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	/* Reboot memory content */
	WRITE_REG(0x24, 0x80);

	k_sleep(K_MSEC(10));

	/* Enable block data update and set scale to +/-4g */
	WRITE_REG(0x23, 0x90);

	/* High-pass filter enabled for INT1 */
	WRITE_REG(0x21, 0x01);

	/* Enable IA1 interrupt on INT1 pin */
	WRITE_REG(0x22, 0x40);

	/* Latch INT1 interrupt request */
	WRITE_REG(0x24, 0x08);

	/* 10 Hz, normal mode, X/Y/Z enabled */
	WRITE_REG(0x20, 0x27);

#undef WRITE_REG

	k_sleep(K_MSEC(50));

	return 0;
}

static int lis2dh12_read(const struct device *dev, float *accel_x, float *accel_y, float *accel_z)
{
	int ret;

	uint8_t write_buf[1];
	uint8_t read_buf[7];

	write_buf[0] = 0xa7;

	ret = ds28e17_i2c_write_read(dev, LIS2DH12_I2C_ADDR, write_buf, 1, read_buf, 7);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	if (!(read_buf[0] & BIT(3))) {
		return -ENODATA;
	}

	int16_t sample_x = sys_get_le16(&read_buf[1]);
	int16_t sample_y = sys_get_le16(&read_buf[3]);
	int16_t sample_z = sys_get_le16(&read_buf[5]);

	if (accel_x) {
		*accel_x = sample_x / 8192.f * 9.80665f;
	}

	if (accel_y) {
		*accel_y = sample_y / 8192.f * 9.80665f;
	}

	if (accel_z) {
		*accel_z = sample_z / 8192.f * 9.80665f;
	}

	return 0;
}

static int lis2dh12_enable_alert(const struct device *dev, int threshold, int duration)
{
	int ret;

	uint8_t write_buf[3];
	uint8_t read_buf[1];

	write_buf[0] = 0x32;
	write_buf[1] = threshold & BIT_MASK(7);
	write_buf[2] = duration & BIT_MASK(7);

	ret = ds28e17_i2c_write(dev, LIS2DH12_I2C_ADDR, write_buf, 3);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	write_buf[0] = 0x26;

	ret = ds28e17_i2c_write_read(dev, LIS2DH12_I2C_ADDR, write_buf, 1, read_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	write_buf[0] = 0x30;
	write_buf[1] = 0x2a;

	ret = ds28e17_i2c_write(dev, LIS2DH12_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int lis2dh12_disable_alert(const struct device *dev)
{
	int ret;

	uint8_t write_buf[2];

	write_buf[0] = 0x30;
	write_buf[1] = 0x00;

	ret = ds28e17_i2c_write(dev, LIS2DH12_I2C_ADDR, write_buf, 2);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int lis2dh12_get_interrupt(const struct device *dev, bool *is_active)
{
	int ret;

	uint8_t write_buf[1];
	uint8_t read_buf[1];

	write_buf[0] = 0x31;

	ret = ds28e17_i2c_write_read(dev, LIS2DH12_I2C_ADDR, write_buf, 1, read_buf, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	*is_active = read_buf[0] & BIT(6) ? true : false;

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

	ret = lis2dh12_init(m_sensors[m_count].dev);
	if (ret) {
		LOG_DBG("Skipping serial number: %llu", serial_number);
		return 0;
	}

	m_sensors[m_count++].serial_number = serial_number;

	LOG_DBG("Registered serial number: %llu", serial_number);

	return 0;
}

int ctr_machine_probe_scan(void)
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

int ctr_machine_probe_get_count(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	int count = m_count;
	k_mutex_unlock(&m_lock);

	return count;
}

#define COMM_PROLOGUE                                                                              \
	int ret;                                                                                   \
	int res = 0;                                                                               \
	if (k_is_in_isr()) {                                                                       \
		return -EWOULDBLOCK;                                                               \
	}                                                                                          \
	k_mutex_lock(&m_lock, K_FOREVER);                                                          \
	if (index < 0 || index >= m_count || !m_count) {                                           \
		k_mutex_unlock(&m_lock);                                                           \
		return -ERANGE;                                                                    \
	}                                                                                          \
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));                     \
	if (!device_is_ready(dev)) {                                                               \
		LOG_ERR("Device not ready");                                                       \
		k_mutex_unlock(&m_lock);                                                           \
		return -ENODEV;                                                                    \
	}                                                                                          \
	if (serial_number) {                                                                       \
		*serial_number = m_sensors[index].serial_number;                                   \
	}                                                                                          \
	ret = ctr_w1_acquire(&m_w1, dev);                                                          \
	if (ret) {                                                                                 \
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);                                  \
		res = ret;                                                                         \
		goto error;                                                                        \
	}                                                                                          \
	if (!device_is_ready(m_sensors[index].dev)) {                                              \
		LOG_ERR("Device not ready");                                                       \
		res = -ENODEV;                                                                     \
		goto error;                                                                        \
	}                                                                                          \
	ret = ds28e17_write_config(m_sensors[index].dev, DS28E17_I2C_SPEED_100_KHZ);               \
	if (ret) {                                                                                 \
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);                            \
		res = ret;                                                                         \
		goto error;                                                                        \
	}

#define COMM_EPILOGUE                                                                              \
	error:                                                                                     \
	ret = ctr_w1_release(&m_w1, dev);                                                          \
	if (ret) {                                                                                 \
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);                                  \
		res = res ? res : ret;                                                             \
	}                                                                                          \
	k_mutex_unlock(&m_lock);                                                                   \
	return res;

int ctr_machine_probe_read_thermometer(int index, uint64_t *serial_number, float *temperature)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	if (temperature) {
		*temperature = NAN;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = tmp112_init(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `tmp112_init` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(TMP112_INIT_TIME);

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
		}
	}

	if (!res && temperature) {
		LOG_DBG("Temperature: %.2f C", *temperature);
	}

	COMM_EPILOGUE
}

int ctr_machine_probe_read_hygrometer(int index, uint64_t *serial_number, float *temperature,
				      float *humidity)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	if (temperature) {
		*temperature = NAN;
	}

	if (humidity) {
		*humidity = NAN;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = sht30_init(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `sht30_init` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(SHT30_INIT_TIME);

	if (!res) {
		ret = sht30_convert(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `sht30_convert` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(SHT30_CONV_TIME);

	if (!res) {
		ret = sht30_read(m_sensors[index].dev, temperature, humidity);
		if (ret) {
			LOG_ERR("Call `sht30_read` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	if (!res && temperature) {
		LOG_DBG("Temperature: %.2f C", *temperature);
	}

	if (!res && humidity) {
		LOG_DBG("Humidity: %.1f %%", *humidity);
	}

	COMM_EPILOGUE
}

int ctr_machine_probe_read_lux_meter(int index, uint64_t *serial_number, float *illuminance)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	if (illuminance) {
		*illuminance = NAN;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = opt3001_init(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `opt3001_init` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(OPT3001_INIT_TIME);

	if (!res) {
		ret = opt3001_convert(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `opt3001_convert` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	k_sleep(OPT3001_CONV_TIME);

	if (!res) {
		ret = opt3001_read(m_sensors[index].dev, illuminance);
		if (ret) {
			LOG_ERR("Call `opt3001_read` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	if (!res && illuminance) {
		LOG_DBG("Illuminance: %.0f lux", *illuminance);
	}

	COMM_EPILOGUE
}

int ctr_machine_probe_read_magnetometer(int index, uint64_t *serial_number, float *magnetic_field)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	if (magnetic_field) {
		*magnetic_field = NAN;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = si7210_read(m_sensors[index].dev, magnetic_field);
		if (ret) {
			LOG_ERR("Call `si7210_read` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	if (!res && magnetic_field) {
		LOG_DBG("Magnetic field: %.2f mT", *magnetic_field);
	}

	COMM_EPILOGUE
}

int ctr_machine_probe_read_accelerometer(int index, uint64_t *serial_number, float *accel_x,
					 float *accel_y, float *accel_z, int *orientation)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	if (accel_x) {
		*accel_x = NAN;
	}

	if (accel_y) {
		*accel_y = NAN;
	}

	if (accel_z) {
		*accel_z = NAN;
	}

	if (orientation) {
		*orientation = INT_MAX;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = lis2dh12_read(m_sensors[index].dev, accel_x, accel_y, accel_z);
		if (ret) {
			LOG_ERR("Call `lis2dh12_read` failed: %d", ret);
			res = ret;
			goto error;
		}
	}

	if (!res && accel_x) {
		LOG_DBG("Acceleration in X-axis: %.3f m/s^2", *accel_x);
	}

	if (!res && accel_y) {
		LOG_DBG("Acceleration in Y-axis: %.3f m/s^2", *accel_y);
	}

	if (!res && accel_z) {
		LOG_DBG("Acceleration in Z-axis: %.3f m/s^2", *accel_z);
	}

	COMM_EPILOGUE

	return 0;
}

int ctr_machine_probe_enable_tilt_alert(int index, uint64_t *serial_number, int threshold,
					int duration)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = lis2dh12_enable_alert(m_sensors[index].dev, threshold, duration);
		if (ret) {
			LOG_ERR("Call `lis2dh12_enable_alert` failed: %d", ret);
			return ret;
		}
	}

	COMM_EPILOGUE

	return 0;
}

int ctr_machine_probe_disable_tilt_alert(int index, uint64_t *serial_number)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = lis2dh12_disable_alert(m_sensors[index].dev);
		if (ret) {
			LOG_ERR("Call `lis2dh12_disable_alert` failed: %d", ret);
			return ret;
		}
	}

	COMM_EPILOGUE

	return 0;
}

int ctr_machine_probe_get_tilt_alert(int index, uint64_t *serial_number, bool *is_active)
{
	if (serial_number) {
		*serial_number = UINT64_MAX;
	}

	if (is_active) {
		*is_active = false;
	}

	COMM_PROLOGUE

	if (!res) {
		ret = lis2dh12_get_interrupt(m_sensors[index].dev, is_active);
		if (ret) {
			LOG_ERR("Call `lis2dh12_get_interrupt` failed: %d", ret);
			return ret;
		}
	}

	if (!res && is_active) {
		LOG_DBG("Alert active: %s", *is_active ? "true" : "false");
	}

	COMM_EPILOGUE

	return 0;
}
