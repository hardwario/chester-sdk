/* CHESTER includes */
#include <ctr_ds18b20.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/w1_sensor.h>
#include <zephyr/drivers/w1.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/zephyr.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_ds18b20, CONFIG_CTR_DS18B20_LOG_LEVEL);

struct sensor {
	uint64_t serial_number;
	const struct device *dev;
};

static struct sensor m_sensors[] = {
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_0)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_1)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_2)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_3)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_4)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_5)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_6)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_7)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_8)) },
	{ .dev = DEVICE_DT_GET(DT_NODELABEL(ds18b20_9)) },
};

static struct k_sem m_lock = Z_SEM_INITIALIZER(m_lock, 0, 1);

static int m_count;

static void w1_search_callback(struct w1_rom rom, void *cb_arg)
{
	int ret;

	struct sensor_value val;
	w1_rom_to_sensor_value(&rom, &val);

	if (m_count >= ARRAY_SIZE(m_sensors)) {
		LOG_WRN("No more space for additional sensor");
		return;
	}

	if (!device_is_ready(m_sensors[m_count].dev)) {
		LOG_ERR("Device not ready");

	} else {
		uint64_t serial_number = sys_get_le48(rom.serial);
		m_sensors[m_count].serial_number = serial_number;

		LOG_DBG("Serial number: %llu", serial_number);

		ret = sensor_attr_set(m_sensors[m_count].dev, SENSOR_CHAN_ALL,
		                      SENSOR_ATTR_W1_ROM_ID, &val);
		if (ret) {
			LOG_WRN("Call `sensor_attr_set` failed: %d", ret);
		}

		m_count++;
	}
}

int ctr_ds18b20_scan(void)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&m_lock, K_FOREVER);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_sem_give(&m_lock);
		return -ENODEV;
	}

	m_count = 0;

	w1_lock_bus(dev);

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		w1_unlock_bus(dev);
		k_sem_give(&m_lock);
		return ret;
	}

	ret = w1_search_rom(dev, w1_search_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Call `w1_search_rom` failed: %d", ret);
		w1_unlock_bus(dev);
		k_sem_give(&m_lock);
		return ret;
	}

	LOG_DBG("Found %d device(s)", ret);

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		w1_unlock_bus(dev);
		k_sem_give(&m_lock);
		return ret;
	}

	w1_unlock_bus(dev);
	k_sem_give(&m_lock);

	return 0;
}

int ctr_ds18b20_get_count(void)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&m_lock, K_FOREVER);
	int count = m_count;
	k_sem_give(&m_lock);

	return count;
}

int ctr_ds18b20_read(int index, uint64_t *serial_number, float *temperature)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_sem_take(&m_lock, K_FOREVER);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ds2484));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_sem_give(&m_lock);
		return -ENODEV;
	}

	int res = 0;

	if (index >= m_count) {
		k_sem_give(&m_lock);
		return -ERANGE;
	}

	if (serial_number) {
		*serial_number = m_sensors[index].serial_number;
	}

	w1_lock_bus(dev);

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		w1_unlock_bus(dev);
		k_sem_give(&m_lock);
		return ret;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		w1_unlock_bus(dev);
		k_sem_give(&m_lock);
		return -ENODEV;
	}

	ret = sensor_sample_fetch(m_sensors[index].dev);
	if (ret) {
		LOG_WRN("Call `sensor_sample_fetch` failed: %d", ret);
		res = ret;

	} else {
		struct sensor_value val;
		ret = sensor_channel_get(m_sensors[index].dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
		if (ret) {
			LOG_WRN("Call `sensor_channel_get` failed: %d", ret);
			res = ret;
		}

		if (temperature) {
			*temperature = (float)val.val1 + (float)val.val2 / 1000000.f;
			LOG_DBG("Temperature: %.2f", *temperature);
		}
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		w1_unlock_bus(dev);
		k_sem_give(&m_lock);
		return ret;
	}

	w1_unlock_bus(dev);
	k_sem_give(&m_lock);

	return res;
}

static int init(const struct device *dev)
{
	LOG_INF("System initialization");

	k_sem_give(&m_lock);

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
