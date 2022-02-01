/*
 * Copyright (c) 2018 Roman Tataurov <diytronic@yandex.ru>
 * Copyright (c) 2021 Thomas Stranger
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Driver for DS18B20 1-Wire temperature sensors
 * A datasheet is available at:
 * https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
 */
#define DT_DRV_COMPAT maxim_ds18b20

#include <device.h>
#include <drivers/w1.h>
#include <drivers/sensor.h>
#include <drivers/sensor/w1_sensor.h>
#include <kernel.h>
#include <sys/__assert.h>
#include <logging/log.h>

#include "ds18b20.h"

LOG_MODULE_REGISTER(DS18B20, CONFIG_SENSOR_LOG_LEVEL);

static int ds18b20_configure(const struct device *dev);

/* measure wait time for 9-bit, 10-bit, 11-bit, 12-bit resolution respectively */
static const uint16_t measure_wait_ms[4] = {94, 188, 376, 750};

static inline void ds18b20_temperature_from_raw(uint8_t *temp_raw,
						struct sensor_value *val)
{
	uint16_t temp = sys_get_le16(temp_raw);

	val->val1 = (temp & 0x0FFF) >> 4;
	val->val2 = (temp & 0x000F) * 10000;
}

/*
 * Write scratch pad, read back, then copy to eeprom
 */
static int ds18b20_write_scratchpad(const struct device *dev,
				    struct ds18b20_scratchpad scratchpad)
{

	struct ds18b20_data *data = dev->data;
	const struct device *bus = ds18b20_bus(dev);
	uint8_t sp_data[3] = {
		scratchpad.alarm_temp_high,
		scratchpad.alarm_temp_low,
		scratchpad.config
	};

	w1_reset_select(bus, &data->rom);
	w1_write_byte(bus, DS18B20_CMD_WRITE_SCRATCHPAD);
	w1_write_block(bus, &sp_data[0], sizeof(sp_data));

	return 0;
}

static int ds18b20_read_scratchpad(const struct device *dev,
				   struct ds18b20_scratchpad *scratchpad)
{
	struct ds18b20_data *data = dev->data;
	const struct device *bus = ds18b20_bus(dev);

	w1_reset_select(bus, &data->rom);
	w1_write_byte(bus, DS18B20_CMD_READ_SCRATCHPAD);
	w1_read_block(bus, (uint8_t *)&scratchpad[0], 9);

	return 0;
}

/* Starts sensor temperature conversion without waiting for completion. */
static void ds18b20_temperature_convert(const struct device *dev)
{
	struct ds18b20_data *data = dev->data;
	const struct device *bus = ds18b20_bus(dev);

	w1_reset_select(bus, &data->rom);
	w1_write_byte(bus, DS18B20_CMD_CONVERT_T);
}

/* Read power supply status from the sensor. */
static int ds18b20_read_power_supply(const struct device *dev)
{
	struct ds18b20_data *data = dev->data;
	const struct device *bus = ds18b20_bus(dev);

	w1_reset_select(bus, &data->rom);
	w1_write_byte(bus, DS18B20_CMD_READ_POWER_SUPPLY);
	data->power_supply = w1_read_bit(bus);

	return 0;
}

/*
 * Write resolution into configuration struct,
 * but don't write it to the sensor yet.
 */
static int ds18b20_set_resolution(const struct device *dev, uint8_t resolution)
{
	struct ds18b20_data *data = dev->data;

	data->scratchpad.config &= ~DS18B20_RESOLUTION_MASK;
	data->scratchpad.config |= DS18B20_RESOLUTION(resolution);

	return 0;
}

static int ds18b20_sample_fetch(const struct device *dev,
				enum sensor_channel chan)
{
	const struct ds18b20_config *cfg = dev->config;
	struct ds18b20_data *data = dev->data;
	int status = 0;

	__ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL ||
			chan == SENSOR_CHAN_AMBIENT_TEMP);

	if (!data->lazy_loaded) {
		status = ds18b20_configure(dev);

		if (status < 0) {
			return status;
		}
		data->lazy_loaded = true;
	}

	ds18b20_temperature_convert(dev);
	k_sleep(K_MSEC(measure_wait_ms[
				DS18B20_RESOLUTION_INDEX(cfg->resolution)]));
	ds18b20_read_scratchpad(dev, &data->scratchpad);

	return 0;
}

static int ds18b20_channel_get(const struct device *dev,
			       enum sensor_channel chan,
			       struct sensor_value *val)
{
	struct ds18b20_data *data = dev->data;

	if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
		ds18b20_temperature_from_raw((uint8_t *)&data->scratchpad.temp,
					     val);
	} else {
		return -ENOTSUP;
	}

	return 0;
}

static int ds18b20_configure(const struct device *dev)
{
	const struct ds18b20_config *cfg = dev->config;
	struct ds18b20_data *data = dev->data;

	/* In single drop configurations the rom can be read from device */
	if (w1_get_peripheral_count(cfg->bus) == 1) {
		if (w1_rom_to_uint64(&data->rom) == 0ULL) {
			w1_read_rom(cfg->bus, &data->rom);
		}
	} else if (w1_rom_to_uint64(&data->rom) == 0ULL) {
		LOG_DBG("nr: %d", w1_get_peripheral_count(cfg->bus));
		LOG_ERR("ROM required, because multiple devices are on the bus");
		return -EINVAL;
	}

	if ((cfg->family != 0) && (cfg->family != data->rom.family)) {
		LOG_ERR("Found 1-Wire device is not a DS18B20");
		return -EINVAL;
	}

	if (!w1_reset_bus(cfg->bus)) {
		LOG_ERR("No 1-Wire devices connected");
		return -EINVAL;
	}

	ds18b20_read_power_supply(dev);
	LOG_DBG("Using external power supply: %d", data->power_supply);

	/* write defaults */
	ds18b20_set_resolution(dev, cfg->resolution);
	ds18b20_write_scratchpad(dev, data->scratchpad);
	LOG_DBG("Init DS18B20: ROM=%016llx\n", w1_rom_to_uint64(&data->rom));

	return 0;
}

int ds18b20_attr_set(const struct device *dev,
		    enum sensor_channel chan,
		    enum sensor_attribute attr,
		    const struct sensor_value *thr)
{
	struct ds18b20_data *data = dev->data;
	int rc = 0;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP) {
		rc = -ENOTSUP;
	} else if ((enum sensor_attribute_w1)attr == SENSOR_ATTR_W1_ROM_ID) {
		data->lazy_loaded = false;
		w1_sensor_value_to_rom(thr, &data->rom);
	};

	return rc;
}

static const struct sensor_driver_api ds18b20_driver_api = {
	.attr_set = ds18b20_attr_set,
	.sample_fetch = ds18b20_sample_fetch,
	.channel_get = ds18b20_channel_get,
};

static int ds18b20_init(const struct device *dev)
{
	const struct ds18b20_config *cfg = dev->config;
	struct ds18b20_data *data = dev->data;

	if (device_is_ready(cfg->bus) ==  0) {
		LOG_DBG("w1 bus is not ready");
		return -ENODEV;
	}

	w1_uint64_to_rom(0ULL, &data->rom);
	data->lazy_loaded = false;
	/* in multi drop configurations the rom is need, but is not set during
	 * driver initialization, therefore do lazy initialization in all cases.
	 */

	return 0;
}

#define DS18B20_CONFIG_INIT(inst)					\
	{								\
		.bus = DEVICE_DT_GET(DT_INST_BUS(inst)),		\
		.family = (uint8_t)DT_INST_REG_ADDR(inst),		\
		.resolution = DT_INST_PROP(inst, resolution),		\
	}

#define DS18B20_DEFINE(inst)						\
	static struct ds18b20_data ds18b20_data_##inst;			\
	static const struct ds18b20_config ds18b20_config_##inst =	\
		DS18B20_CONFIG_INIT(inst);				\
	DEVICE_DT_INST_DEFINE(inst,					\
			      ds18b20_init,				\
			      NULL,					\
			      &ds18b20_data_##inst,			\
			      &ds18b20_config_##inst,			\
			      POST_KERNEL,				\
			      CONFIG_SENSOR_INIT_PRIORITY,		\
			      &ds18b20_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DS18B20_DEFINE)
