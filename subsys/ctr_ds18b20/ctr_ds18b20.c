/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_w1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/w1_sensor.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_ds18b20, CONFIG_CTR_DS18B20_LOG_LEVEL);

struct sensor {
	uint64_t serial_number;
	const struct device *dev;
};

static K_MUTEX_DEFINE(m_lock);

static struct ctr_w1 m_w1;

static struct sensor m_sensors[] = {
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_0))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_1))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_2))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_3))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_4))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_5))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_6))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_7))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_8))},
	{.dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_9))},
};

static int m_count;

static int scan_callback(struct w1_rom rom, void *user_data)
{
	int ret;

	if (rom.family != 0x28) {
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

	struct sensor_value val;
	w1_rom_to_sensor_value(&rom, &val);

	ret = sensor_attr_set(m_sensors[m_count].dev, SENSOR_CHAN_ALL, SENSOR_ATTR_W1_ROM, &val);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
		return ret;
	}

	m_sensors[m_count++].serial_number = serial_number;

	LOG_DBG("Registered serial number: %llu", serial_number);

	return 0;
}

int ctr_ds18b20_scan(void)
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

int ctr_ds18b20_get_count(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);
	int count = m_count;
	k_mutex_unlock(&m_lock);

	return count;
}

int ctr_ds18b20_read(int index, uint64_t *serial_number, float *temperature)
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

	ret = sensor_sample_fetch(m_sensors[index].dev);
	if (ret) {
		LOG_WRN("Call `sensor_sample_fetch` failed: %d", ret);
		res = ret;
		goto error;
	}

	struct sensor_value val;
	ret = sensor_channel_get(m_sensors[index].dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (ret) {
		LOG_WRN("Call `sensor_channel_get` failed: %d", ret);
		res = ret;
	}

	if (temperature) {
		*temperature = (float)val.val1 + (float)val.val2 / 1000000.f;
		LOG_DBG("Temperature: %.2f C", *temperature);
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
