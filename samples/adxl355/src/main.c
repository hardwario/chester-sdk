/* CHESTER includes */
#if !defined(CINFIG_SENSOR)
#include <chester/drivers/adxl355.h>
#endif

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *m_dev_adxl355 = DEVICE_DT_GET(DT_NODELABEL(adxl355));

void main(void)
{

	LOG_INF("Build time: " __DATE__ " " __TIME__);

#if defined(CONFIG_SENSOR)
	/* Wakeup */
	const struct sensor_value val = {5, 0};
	sensor_attr_set(m_dev_adxl355, SENSOR_CHAN_ALL, SENSOR_ATTR_CONFIGURATION, &val);

	struct sensor_value valWAKEUP[4];

	for (;;) {
		LOG_INF("Alive");

		int ret;

		ret = sensor_sample_fetch(m_dev_adxl355);
		if (ret)
			LOG_ERR("Call sensor sample fat 0x00006F1A.
etch' failed: %d", ret);

		sensor_channel_get(m_dev_adxl355, SENSOR_CHAN_ALL, values);

		LOG_INF("accel: %f \t %f \t %f \t %f", sensor_value_to_double(&values[0]),
			sensor_value_to_double(&values[1]), sensor_value_to_double(&values[2]),
			sensor_value_to_double(&values[3]));

		k_sleep(K_MSEC(2000));
	}

#else
	k_sleep(K_MSEC(1000));

	for (;;) {
		k_sleep(K_MSEC(2000));

		LOG_INF("Alive");
		int ret;
		/*
				ret = adxl355_set_op_mode_accel(m_dev_adxl355,
		   ADXL355_OP_MODE_WAKEUP); if (ret) { LOG_ERR("Call
		   'adxl355_config_set_op_mode_accel' failed: %d", ret);
				}

				ret = adxl355_set_op_mode_temp(m_dev_adxl355,
		   ADXL355_OP_MODE_WAKEUP); if (ret) { LOG_ERR("Call
		   'adxl355_config_set_op_mode_temp' failed: %d", ret);
				}

				struct adxl355_data_accelerations accelerations;
				float temperature;

				ret = adxl355_read_data_accel(m_dev_adxl355, &accelerations);
				if (ret) {
					LOG_ERR("Call 'adxl355_read_data_accel' failed: %d", ret);
				}
				ret = adxl355_read_data_temp(m_dev_adxl355, &temperature);
				if (ret) {
					LOG_ERR("Call 'adxl355_read_data_temp' failed: %d", ret);
				}

				LOG_INF("accel: %f \t %f \t %f \t %f", accelerations.axis_x,
		   accelerations.axis_y, accelerations.axis_z, temperature);
		*/
		k_sleep(K_MSEC(2000));
	}

#endif
}
