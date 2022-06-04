/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/w1_sensor.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *dev_bus = DEVICE_DT_GET(DT_NODELABEL(ds2484));
const struct device *dev_snsr[] = {
	DEVICE_DT_GET(DT_NODELABEL(ds18b20_0)),
	DEVICE_DT_GET(DT_NODELABEL(ds18b20_1)),
};

void search_callback(struct w1_rom rom, void *cb_arg)
{
	int ret;

	static int i = 0;

	struct sensor_value val;
	w1_rom_to_sensor_value(&rom, &val);

	if (i < ARRAY_SIZE(dev_snsr)) {
		ret = sensor_attr_set(dev_snsr[i++], SENSOR_CHAN_ALL, SENSOR_ATTR_W1_ROM_ID, &val);
		if (ret) {
			LOG_WRN("Call `sensor_attr_set` failed: %d", ret);
		}
	}
}

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	w1_lock_bus(dev_bus);
	pm_device_action_run(dev_bus, PM_DEVICE_ACTION_RESUME);

	size_t num_devices = w1_search_rom(dev_bus, search_callback, NULL);
	LOG_INF("Number of devices: %u", num_devices);

	pm_device_action_run(dev_bus, PM_DEVICE_ACTION_SUSPEND);
	w1_unlock_bus(dev_bus);

	k_sleep(K_SECONDS(1));

	for (;;) {
		struct sensor_value temp;

		w1_lock_bus(dev_bus);
		pm_device_action_run(dev_bus, PM_DEVICE_ACTION_RESUME);

		for (int j = 0; j < ARRAY_SIZE(dev_snsr); j++) {
			sensor_sample_fetch(dev_snsr[j]);
			sensor_channel_get(dev_snsr[j], SENSOR_CHAN_AMBIENT_TEMP, &temp);
			LOG_INF("Temp %d: %d.%06d", j, temp.val1, temp.val2);
		}

		pm_device_action_run(dev_bus, PM_DEVICE_ACTION_SUSPEND);
		w1_unlock_bus(dev_bus);

		k_sleep(K_SECONDS(30));
	}
}
