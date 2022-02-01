/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <drivers/w1.h>
#include <drivers/sensor/w1_sensor.h>
#include <logging/log.h>
#include <zephyr.h>

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

	size_t num_devices = w1_search_rom(dev_bus, search_callback, NULL);
	LOG_INF("Number of devices: %u", num_devices);

	w1_unlock_bus(dev_bus);

	k_sleep(K_SECONDS(1));

	for (;;) {
		struct sensor_value temp;

		for (int j = 0; j < ARRAY_SIZE(dev_snsr); j++) {
			sensor_sample_fetch(dev_snsr[j]);
			sensor_channel_get(dev_snsr[j], SENSOR_CHAN_AMBIENT_TEMP, &temp);
			LOG_INF("Temp %d: %d.%06d\n", j, temp.val1, temp.val2);
		}

		k_sleep(K_SECONDS(10));
	}
}
