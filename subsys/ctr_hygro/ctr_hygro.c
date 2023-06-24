/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_hygro.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>

BUILD_ASSERT(IS_ENABLED(CONFIG_SHT3XD_SINGLE_SHOT_MODE),
	     "Option SHT3XD_SINGLE_SHOT_MODE has to be chosen");

LOG_MODULE_REGISTER(ctr_hygro, CONFIG_CTR_HYGRO_LOG_LEVEL);

static K_MUTEX_DEFINE(m_mut);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(sht30_ext));

int ctr_hygro_read(float *temperature, float *humidity)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `SHT30_EXT` not ready");
		k_mutex_unlock(&m_mut);
		return -EINVAL;
	}

	ret = sensor_sample_fetch(m_dev);
	if (ret) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val_temperature;
	ret = sensor_channel_get(m_dev, SENSOR_CHAN_AMBIENT_TEMP, &val_temperature);
	if (ret) {
		LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float temperature_ = sensor_value_to_double(&val_temperature);

	LOG_DBG("Temperature: %.2f C", temperature_);

	if (temperature != NULL) {
		*temperature = temperature_;
	}

	struct sensor_value val_humidity;
	ret = sensor_channel_get(m_dev, SENSOR_CHAN_HUMIDITY, &val_humidity);
	if (ret) {
		LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float humidity_ = sensor_value_to_double(&val_humidity);

	LOG_DBG("Humidity: %.1f %%", humidity_);

	if (humidity != NULL) {
		*humidity = humidity_;
	}

	k_mutex_unlock(&m_mut);

	return 0;
}
