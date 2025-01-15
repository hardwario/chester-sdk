/*
 * Copyright (c) 2024 HARDWARIO a.s.
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
#include <zephyr/sys/crc.h>

/* Standard includes */
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* CHESTER includes */
#include <chester/drivers/sensor/sps30.h>

LOG_MODULE_REGISTER(sps30, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT sensirion_sps30

struct sps30_data {
	uint32_t interval;
	struct k_mutex data_lock;

	float num_conc_pm_0_5;
	float typ_part_size;
	float num_conc_pm_1_0;
	float num_conc_pm_2_5;
	float num_conc_pm_4_0;
	float num_conc_pm_10_0;

	float mass_conc_pm_1_0;
	float mass_conc_pm_2_5;
	float mass_conc_pm_4_0;
	float mass_conc_pm_10_0;
};

struct sps30_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
};

#define SPS30_PTR_START_MEASUREMENT 0x0010
#define SPS30_PTR_PRODUCT_TYPE      0xD002
#define SPS30_PTR_SERIAL_NO         0xD033
#define SPS30_PTR_READ_DATA         0x0300

#define SPS30_MAX_WRITE_WORDS 2
#define SPS30_MAX_READ_WORDS  20

static inline const struct sps30_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct sps30_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int sps30_set_pointer(const struct device *dev, uint16_t pointer)
{
	int ret;

	LOG_DBG("Set Pointer: 0x%04x", pointer);

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	uint8_t buf[2] = {(pointer & 0xFF00) >> 8, pointer & 0xFF};

	ret = i2c_write(get_config(dev)->i2c_dev, buf, sizeof(buf), get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static uint8_t sps30_crc(uint16_t value)
{
	uint8_t buff[2] = {(value & 0xFF00) >> 8, value & 0xFF};

	return crc8(buff, sizeof(buff), 0x31, 0xFF, false);
}

static int sps30_write(const struct device *dev, uint16_t pointer, uint16_t *data, size_t data_len)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	uint8_t buff[sizeof(uint16_t) + SPS30_MAX_WRITE_WORDS * 3];
	if (data_len > SPS30_MAX_WRITE_WORDS * sizeof(uint16_t)) {
		LOG_ERR("Write message too large");
		return -EINVAL;
	}

	uint8_t *buff_wr = buff;

	*buff_wr++ = (pointer & 0xFF00) >> 8;
	*buff_wr++ = pointer & 0xFF;

	for (size_t i = 0; i < data_len / sizeof(uint16_t); i++) {
		*buff_wr++ = (data[i] & 0xFF00) >> 8;
		*buff_wr++ = data[i] & 0xFF;
		*buff_wr++ = sps30_crc(data[i]);
	}

	LOG_DBG("Set write: 0x%04x", pointer);

	ret = i2c_write(get_config(dev)->i2c_dev, buff, buff_wr - buff, get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int sps30_read(const struct device *dev, uint16_t pointer, uint16_t *data, size_t data_len)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	if (data_len > SPS30_MAX_READ_WORDS * sizeof(uint16_t)) {
		LOG_ERR("Unsupported read buffer length");
		return -EINVAL;
	}

	ret = sps30_set_pointer(dev, pointer);
	if (ret) {
		LOG_ERR("Call `sps30_set_pointer` failed: %d", ret);
		return ret;
	}

	uint8_t rd_buff[SPS30_MAX_READ_WORDS * 3];
	size_t rd_len = 3 * (data_len / sizeof(uint16_t));

	ret = i2c_read(get_config(dev)->i2c_dev, rd_buff, rd_len, get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_read` failed: %d", ret);
		return ret;
	}

	for (size_t i = 0; i < data_len / sizeof(uint16_t); i++) {
		uint16_t b1 = rd_buff[i * 3 + 0];
		uint16_t b2 = rd_buff[i * 3 + 1];
		uint8_t b3 = rd_buff[i * 3 + 2];

		uint16_t val = (b1 << 8) | b2;

		uint8_t computed = sps30_crc(val);

		if (computed != b3) {
			LOG_ERR("Invalid CRC. Message: 0x%02x Computed: 0x%02x.\n", b3, computed);
			return -EIO;
		}

		data[i] = val;
	}

	return 0;
}

static int sps30_read_ascii(const struct device *dev, uint16_t pointer, char *data, size_t data_len)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	if (data_len % 2 != 0 || data_len > SPS30_MAX_READ_WORDS * sizeof(uint16_t)) {
		LOG_ERR("Unsupported read buffer length");
		return -EINVAL;
	}

	ret = sps30_set_pointer(dev, pointer);
	if (ret) {
		LOG_ERR("Call `sps30_set_pointer` failed: %d", ret);
		return ret;
	}

	uint8_t rd_buff[SPS30_MAX_READ_WORDS * 3];
	size_t rd_len = 3 * (data_len / sizeof(uint16_t));

	ret = i2c_read(get_config(dev)->i2c_dev, rd_buff, rd_len, get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write_read` failed: %d", ret);
		return ret;
	}

	uint8_t *rd_buff_ptr = rd_buff;

	for (size_t i = 0; i < data_len / sizeof(uint16_t); i++) {
		uint8_t byte1 = *rd_buff_ptr++;
		uint8_t byte2 = *rd_buff_ptr++;
		uint8_t byte3 = *rd_buff_ptr++;
		uint16_t val = (((uint16_t)byte1) << 8) | (uint16_t)byte2;
		uint8_t computed = sps30_crc(val);

		if (computed != byte3) {
			LOG_ERR("Invalid CRC Message: 0x%02x Computed: 0x%02x.\n", byte3, computed);
			return -EIO;
		}

		data[i * 2 + 0] = byte1;
		data[i * 2 + 1] = byte2;
	}

	return 0;
}

static int sps30_start_measurement(const struct device *dev)
{
	int ret;

	uint16_t param = 0x0300;
	ret = sps30_write(dev, SPS30_PTR_START_MEASUREMENT, &param, sizeof(param));
	if (ret) {
		LOG_ERR("Call `sps30_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int sps30_configure(const struct device *dev)
{
	int ret;

	char product_type[9];
	char serial_no[32];

	ret = sps30_read_ascii(dev, SPS30_PTR_PRODUCT_TYPE, product_type, sizeof(product_type) - 1);
	if (ret) {
		LOG_ERR("Call `sps30_read_ascii` (prod type req) failed: %d", ret);
		return ret;
	}

	product_type[sizeof(product_type) - 1] = '\0';

	if (strcmp(product_type, "00080000") != 0) {
		LOG_ERR("Unexpected product type: %s", product_type);
		return -ENODEV;
	}

	ret = sps30_read_ascii(dev, SPS30_PTR_SERIAL_NO, serial_no, sizeof(serial_no));
	if (ret) {
		LOG_ERR("Call `sps30_read_ascii` (serial no req) failed: %d", ret);
		return ret;
	}

	serial_no[sizeof(serial_no) - 1] = '\0';

	LOG_INF("Detected SPS30 sensor. Serial no: %s", serial_no);

	ret = sps30_start_measurement(dev);
	if (ret) {
		LOG_ERR("Call `sps30_start_measurement` failed: %d", ret);
		return ret;
	}

	return 0;
}

int sps30_power_on_init(const struct device *dev)
{
	int ret = sps30_configure(dev);
	if (ret) {
		LOG_ERR("Call `sps30_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

static float sps30_parse_float(uint16_t *u)
{
	union {
		float f;
		uint32_t u;
	} convert;

	uint32_t u16_1 = *u++;
	uint32_t u16_2 = *u++;

	convert.u = (u16_1 << 16) | u16_2;

	return convert.f;
}

static int sps30_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	int ret;

	k_mutex_lock(&get_data(dev)->data_lock, K_FOREVER);

	struct sps30_data *data = get_data(dev);

	uint16_t buff[20];
	ret = sps30_read(dev, SPS30_PTR_READ_DATA, buff, sizeof(buff));
	if (ret) {
		LOG_ERR("Call `sps30_read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->data_lock);
		return ret;
	}

	if (chan == SENSOR_CHAN_PM_1_0 || chan == SENSOR_CHAN_ALL) {
		data->mass_conc_pm_1_0 = sps30_parse_float(buff + 0);
	}
	if (chan == SENSOR_CHAN_PM_2_5 || chan == SENSOR_CHAN_ALL) {
		data->mass_conc_pm_2_5 = sps30_parse_float(buff + 2);
	}
	if ((int16_t)chan == SENSOR_CHAN_SPS30_PM_4_0 || chan == SENSOR_CHAN_ALL) {
		data->mass_conc_pm_4_0 = sps30_parse_float(buff + 4);
	}
	if (chan == SENSOR_CHAN_PM_10 || chan == SENSOR_CHAN_ALL) {
		data->mass_conc_pm_10_0 = sps30_parse_float(buff + 6);
	}

	if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_0_5 ||
	    chan == SENSOR_CHAN_ALL) {
		data->num_conc_pm_0_5 = sps30_parse_float(buff + 8);
	}
	if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_1_0 ||
	    chan == SENSOR_CHAN_ALL) {
		data->num_conc_pm_1_0 = sps30_parse_float(buff + 10);
	}
	if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_2_5 ||
	    chan == SENSOR_CHAN_ALL) {
		data->num_conc_pm_2_5 = sps30_parse_float(buff + 12);
	}
	if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_4_0 ||
	    chan == SENSOR_CHAN_ALL) {
		data->num_conc_pm_4_0 = sps30_parse_float(buff + 14);
	}
	if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_10_0 ||
	    chan == SENSOR_CHAN_ALL) {
		data->num_conc_pm_10_0 = sps30_parse_float(buff + 16);
	}
	if ((int16_t)chan == SENSOR_CHAN_SPS30_TYP_PART_SIZE || chan == SENSOR_CHAN_ALL) {
		data->typ_part_size = sps30_parse_float(buff + 18);
	}

	k_mutex_unlock(&get_data(dev)->data_lock);
	return 0;
}

static int sps30_channel_get(const struct device *dev, enum sensor_channel chan,
			     struct sensor_value *val)
{
	k_mutex_lock(&get_data(dev)->data_lock, K_FOREVER);

	if (chan == SENSOR_CHAN_PM_1_0) {
		sensor_value_from_double(val, get_data(dev)->mass_conc_pm_1_0);
	} else if (chan == SENSOR_CHAN_PM_2_5) {
		sensor_value_from_double(val, get_data(dev)->mass_conc_pm_2_5);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_PM_4_0) {
		sensor_value_from_double(val, get_data(dev)->mass_conc_pm_4_0);
	} else if (chan == SENSOR_CHAN_PM_10) {
		sensor_value_from_double(val, get_data(dev)->mass_conc_pm_10_0);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_0_5) {
		sensor_value_from_double(val, get_data(dev)->num_conc_pm_0_5);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_1_0) {
		sensor_value_from_double(val, get_data(dev)->num_conc_pm_1_0);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_2_5) {
		sensor_value_from_double(val, get_data(dev)->num_conc_pm_2_5);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_4_0) {
		sensor_value_from_double(val, get_data(dev)->num_conc_pm_4_0);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_NUM_CONCENTRATION_PM_10_0) {
		sensor_value_from_double(val, get_data(dev)->num_conc_pm_10_0);
	} else if ((int16_t)chan == SENSOR_CHAN_SPS30_TYP_PART_SIZE) {
		sensor_value_from_double(val, get_data(dev)->typ_part_size);
	} else {
		k_mutex_unlock(&get_data(dev)->data_lock);
		return -ENOTSUP;
	}

	k_mutex_unlock(&get_data(dev)->data_lock);
	return 0;
}

static int sps30_init(const struct device *dev)
{
	struct sps30_data *data = get_data(dev);

	k_mutex_init(&data->data_lock);

	return 0;
}

static const struct sensor_driver_api sps30_driver_api = {
	.sample_fetch = sps30_sample_fetch,
	.channel_get = sps30_channel_get,
};

#define SPS30_INIT(n)                                                                              \
	static const struct sps30_config inst_##n##_config = {                                     \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                          \
		.i2c_addr = DT_INST_REG_ADDR(n),                                                   \
	};                                                                                         \
	static struct sps30_data inst_##n##_data;                                                  \
	SENSOR_DEVICE_DT_INST_DEFINE(n, sps30_init, NULL, &inst_##n##_data, &inst_##n##_config,    \
				     POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &sps30_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SPS30_INIT)
