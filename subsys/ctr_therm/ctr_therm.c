/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_therm.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_therm, CONFIG_CTR_THERM_LOG_LEVEL);

static struct k_sem m_lock = Z_SEM_INITIALIZER(m_lock, 0, 1);
static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(tmp112));

int ctr_therm_read(float *temperature)
{
	int ret;

	k_sem_take(&m_lock, K_FOREVER);

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		k_sem_give(&m_lock);
		return -ENODEV;
	}

	ret = sensor_sample_fetch(m_dev);
	if (ret) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_sem_give(&m_lock);
		return ret;
	}

	struct sensor_value val;
	ret = sensor_channel_get(m_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (ret) {
		LOG_ERR("Call `sensor_channel_get` failed:t %d", ret);
		k_sem_give(&m_lock);
		return ret;
	}

	float temperature_ = sensor_value_to_double(&val);

	LOG_DBG("Temperature: %.2f C", temperature_);

	if (temperature != NULL) {
		*temperature = temperature_;
	}

	k_sem_give(&m_lock);

	return 0;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		k_sem_give(&m_lock);
		return -ENODEV;
	}

	struct sensor_value val_scale = {
		.val1 = 128,
		.val2 = 0,
	};
	ret = sensor_attr_set(m_dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_FULL_SCALE, &val_scale);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed (SENSOR_ATTR_FULL_SCALE): %d", ret);
		m_dev->state->initialized = false;
		k_sem_give(&m_lock);
		return ret;
	}

	struct sensor_value val_freq = {
		.val1 = 0,
		.val2 = 250000,
	};
	ret = sensor_attr_set(m_dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_SAMPLING_FREQUENCY,
			      &val_freq);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed (SENSOR_ATTR_SAMPLING_FREQUENCY): %d", ret);
		m_dev->state->initialized = false;
		k_sem_give(&m_lock);
		return ret;
	}

	k_sem_give(&m_lock);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
