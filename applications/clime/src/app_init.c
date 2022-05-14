#include "app_init.h"
#include "app_data.h"
#include "app_handler.h"

/* CHESTER includes */
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_therm.h>
#include <ctr_wdog.h>
#include <drivers/sensor/w1_sensor.h>
#include <drivers/w1.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

K_SEM_DEFINE(g_app_init_sem, 0, 1);

struct ctr_wdog_channel g_app_wdog_channel;

const struct device *g_app_ds18b20_dev[W1_THERM_COUNT] = {
	DEVICE_DT_GET(DT_NODELABEL(ds18b20_0)),
};

static void w1_search_callback(struct w1_rom rom, void *cb_arg)
{
	int ret;

	struct sensor_value val;
	w1_rom_to_sensor_value(&rom, &val);

	static int idx = 0;
	if (idx < W1_THERM_COUNT) {
		struct w1_therm *therm = &g_app_data.w1_therm[idx];

		therm->serial_number = sys_get_le48(rom.serial);

		ret = sensor_attr_set(g_app_ds18b20_dev[idx++], SENSOR_CHAN_ALL,
		                      SENSOR_ATTR_W1_ROM_ID, &val);
		if (ret) {
			LOG_WRN("Call `sensor_attr_set` failed: %d", ret);
		}
	}
}

static int init_w1(void)
{
	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	w1_lock_bus(dev);

	size_t num_devices = w1_search_rom(dev, w1_search_callback, NULL);
	LOG_INF("Number of 1-Wire devices: %u", num_devices);

	w1_unlock_bus(dev);

	return 0;
}

int app_init(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_install(&g_app_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		return ret;
	}

	ret = ctr_therm_init();
	if (ret) {
		LOG_ERR("Call `ctr_therm_init` failed: %d", ret);
		return ret;
	}

	ret = init_w1();
	if (ret) {
		LOG_ERR("Call `init_w1` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_set_event_cb(app_handler_lte, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		return ret;
	}

	for (;;) {
		ret = ctr_wdog_feed(&g_app_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			return ret;
		}

		ret = k_sem_take(&g_app_init_sem, K_SECONDS(1));
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			return ret;
		}

		break;
	}

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	return 0;
}
