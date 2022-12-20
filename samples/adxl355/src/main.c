/* CHESTER includes */
#include <chester/drivers/adxl355.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

// const struct device *dev = DEVICE_DT_GET( DT_NODELABEL(adxl355_a) );
const struct device *m_dev_adxl355 = DEVICE_DT_GET(DT_NODELABEL(adxl355));

void main(void)
{

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	/* Set standby off */
	const struct sensor_value val = {5, 0};
	sensor_attr_set(m_dev_adxl355, SENSOR_CHAN_ALL, SENSOR_ATTR_CONFIGURATION, &val);

	struct sensor_value values[4];

	for (;;) {
		LOG_INF("Alive");

		int ret;

		ret = sensor_sample_fetch(m_dev_adxl355);
		if (ret)
			LOG_ERR("Call sensor sample fetch' failed: %d", ret);

		sensor_channel_get(m_dev_adxl355, SENSOR_CHAN_ALL, values);

		LOG_INF("accel: %f \t %f \t %f \t %f", sensor_value_to_double(&values[0]),
			sensor_value_to_double(&values[1]), sensor_value_to_double(&values[2]),
			sensor_value_to_double(&values[3]));

		k_sleep(K_MSEC(1000));
	}
}
