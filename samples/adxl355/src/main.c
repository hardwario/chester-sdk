/* CHESTER includes */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(adxl355_b));

#if !defined(CONFIG_ADXL355_TRIGGER)

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device %s is not ready\n", dev->name);
		return;
	}

	int ret;

	/* WakeUp */
	const struct sensor_value val = {4, 0};
	sensor_attr_set(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_CONFIGURATION, &val);

	for (;;) {
		LOG_INF("Alive");

		k_sleep(K_MSEC(1000));
		if (sensor_sample_fetch(dev) < 0) {
			LOG_ERR("Sample fetch error\n");
			return;
		}

		ret = sensor_sample_fetch(dev);
		if (ret) {
			LOG_ERR("Call sensor sample fatetch' failed: %d", ret);
		}

		struct sensor_value values[4];
		sensor_channel_get(dev, SENSOR_CHAN_ALL, values);

		LOG_INF("accel: %f \t %f \t %f \t %f", sensor_value_to_double(&values[0]),
			sensor_value_to_double(&values[1]), sensor_value_to_double(&values[2]),
			sensor_value_to_double(&values[3]));
	}
}

#elif defined(CONFIG_ADXL355_TRIGGER) && !defined(CONFIG_ADXL355_FIFO)

K_SEM_DEFINE(sem, 0, 1);

static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	int ret;

	/* fetch samples */
	ret = sensor_sample_fetch(dev);
	if (ret) {
		LOG_ERR("Call sensor sample fatetch' failed: %d", ret);
	}
	k_sem_give(&sem);
}

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device %s is not ready\n", dev->name);
		return;
	}

	int ret;

	/* set trigger handler */
	struct sensor_trigger trig = {.chan = SENSOR_CHAN_ACCEL_XYZ,
				      .type = SENSOR_TRIG_DATA_READY};

	ret = sensor_trigger_set(dev, &trig, trigger_handler);
	if (ret) {
		LOG_ERR("Call 'sensor_trigger_set' failed: %d", ret);
		return;
	}

	/* WakeUp */
	struct sensor_value val = {4, 0};
	sensor_attr_set(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_CONFIGURATION, &val);

	for (;;) {

		/* wait to data ready */
		k_sem_take(&sem, K_FOREVER);

		/* get samples */
		struct sensor_value values[4];
		sensor_channel_get(dev, SENSOR_CHAN_ALL, values);

		LOG_INF("accel: %f \t %f \t %f \t %f", sensor_value_to_double(&values[0]),
			sensor_value_to_double(&values[1]), sensor_value_to_double(&values[2]),
			sensor_value_to_double(&values[3]));
	}
}

#elif defined(CONFIG_ADXL355_TRIGGER) && defined(CONFIG_ADXL355_FIFO)

K_SEM_DEFINE(sem, 0, 1);

static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	int ret;

	switch (trig->type) {
	case SENSOR_TRIG_DATA_READY:
		/* fetch data */
		ret = sensor_sample_fetch(dev);
		if (ret) {
			LOG_ERR("Call sensor sample fatetch' failed: %d", ret);
		}
		k_sem_give(&sem);
		break;
	default:
		break;
	}
}

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device %s is not ready\n", dev->name);
		return;
	}

	int ret;

	/* set trigger handler */
	struct sensor_trigger trig = {.chan = SENSOR_CHAN_ACCEL_XYZ,
				      .type = SENSOR_TRIG_DATA_READY};
	ret = sensor_trigger_set(dev, &trig, trigger_handler);
	if (ret) {
		LOG_ERR("Call 'sensor_trigger_set' failed: %d", ret);
		return;
	}

	/* WakeUp */
	struct sensor_value val = {4, 0};
	sensor_attr_set(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_CONFIGURATION, &val);

	long sample = 0;

	for (;;) {

		/* wait to data ready */
		k_sem_take(&sem, K_FOREVER);

		/* get count of accelerations array samples */
		struct sensor_value samples;
		sensor_attr_get(dev, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_COMMON_COUNT, &samples);

		/* get accelerations array*/
		struct sensor_value values[samples.val1 * 3];
		sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, values);

		/* get temperature */
		struct sensor_value value_temp;
		sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &value_temp);

		/* Print accelerations array in g */
		for (int i = 0; i < ARRAY_SIZE(values);) {
			LOG_INF("accel: %ld | %f \t %f \t %f \t %f", sample,
				sensor_value_to_double(&values[i]),
				sensor_value_to_double(&values[i + 1]),
				sensor_value_to_double(&values[i + 2]),
				sensor_value_to_double(&value_temp));

			i += 3;
			sample++;
		}
	}
}
#endif
