/* CHESTER includes */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(adxl355_b));

K_SEM_DEFINE(sem, 0, 1);

static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	LOG_INF("%d", trig->type);
}

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (!device_is_ready(dev)) {
		printf("Device %s is not ready\n", dev->name);
		return;
	}

	int ret;

	if (IS_ENABLED(CONFIG_ADXL355_TRIGGER)) {
		/* set trigger handler */

		struct sensor_trigger trig = {.chan = SENSOR_CHAN_ACCEL_XYZ};

		trig.type = SENSOR_TRIG_DATA_READY;
		ret = sensor_trigger_set(dev, &trig, trigger_handler);
		if (ret) {
			LOG_ERR("Call 'sensor_trigger_set' failed: %d", ret);
			return ret;
		}
	}

	/* WakeUp */
	const struct sensor_value val = {5, 0};
	sensor_attr_set(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_CONFIGURATION, &val);

	struct sensor_value values[4];

	for (;;) {

		LOG_INF("Alive");

		if (IS_ENABLED(CONFIG_ADXL355_TRIGGER)) {
			k_sem_take(&sem, K_FOREVER);
		} else {
			k_sleep(K_MSEC(1000));
			if (sensor_sample_fetch(dev) < 0) {
				printf("Sample fetch error\n");
				return;
			}
		}

		ret = sensor_sample_fetch(dev);
		if (ret) {
			LOG_ERR("Call sensor sample fatetch' failed: %d", ret);
		}

		sensor_channel_get(dev, SENSOR_CHAN_ALL, values);

		LOG_INF("accel: %f \t %f \t %f \t %f", sensor_value_to_double(&values[0]),
			sensor_value_to_double(&values[1]), sensor_value_to_double(&values[2]),
			sensor_value_to_double(&values[3]));
	}
}
