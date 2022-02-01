/*
 * Copyright (c) 2021 Thomas Stranger
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_DS18B20_DS18B20_H_
#define ZEPHYR_DRIVERS_SENSOR_DS18B20_DS18B20_H_

#include <device.h>
#include <devicetree.h>
#include <kernel.h>
#include <drivers/gpio.h>
#include <drivers/w1.h>

#define DS18B20_CMD_CONVERT_T         0x44
#define DS18B20_CMD_WRITE_SCRATCHPAD  0x4E
#define DS18B20_CMD_READ_SCRATCHPAD   0xBE
#define DS18B20_CMD_COPY_SCRATCHPAD   0x48
#define DS18B20_CMD_RECALL_EEPROM     0xB8
#define DS18B20_CMD_READ_POWER_SUPPLY 0xB4

 /* Device is powered by an external source */
#define DS18B20_POWER_SUPPLY_EXTERNAL 0x1
 /* Device is powered by "parasite" power from the w1-bus */
#define DS18B20_POWER_SUPPLY_PARASITE 0x0

/* resolution is set using bit 5 and 6 of configuration register
 * macro only valid for values 9 to 12
 */
#define DS18B20_RESOLUTION_MASK		(3 << 5)
/* convert resolution in bits to scratchpad config format */
#define DS18B20_RESOLUTION(res)		((res - 9) << 5)
/* convert resolution in bits to array index (for resolution specific elements) */
#define DS18B20_RESOLUTION_INDEX(res)	((res - 9))

struct ds18b20_scratchpad {
	int16_t temp;
	uint8_t alarm_temp_high;
	uint8_t alarm_temp_low;
	uint8_t config;
	uint8_t res[3];
	uint8_t crc;
};

struct ds18b20_config {
	const struct device *bus;
	uint8_t family;
	uint8_t resolution;
};

struct ds18b20_data {
	struct w1_rom rom;
	struct ds18b20_scratchpad scratchpad;
	uint8_t power_supply;
	bool lazy_loaded;
};

static inline const struct device *ds18b20_bus(const struct device *dev)
{
	const struct ds18b20_config *dcp = dev->config;

	return dcp->bus;
}

#endif /* ZEPHYR_DRIVERS_SENSOR_DS18B20_DS18B20_H_ */
