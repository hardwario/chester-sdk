/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_barometer_tag));

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	for (;;) {
		LOG_INF("Alive");

		if (!device_is_ready(m_dev)) {
			LOG_ERR("Device not ready");
			break;
		}

		ret = sensor_sample_fetch(m_dev);
		if (ret) {
			LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
			break;
		}

		struct sensor_value val;

		ret = sensor_channel_get(m_dev, SENSOR_CHAN_ALTITUDE, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			break;
		}
		float altitude_ = sensor_value_to_double(&val);

		ret = sensor_channel_get(m_dev, SENSOR_CHAN_PRESS, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			break;
		}
		float pressure_ = sensor_value_to_double(&val);

		ret = sensor_channel_get(m_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
		if (ret) {
			LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
			break;
		}
		float temperature_ = sensor_value_to_double(&val);

		LOG_DBG("Altitude: %.0f m Pressure: %.0f Pa Temperature: %.2f", altitude_,
			pressure_, temperature_);

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
