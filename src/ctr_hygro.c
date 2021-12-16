#include <ctr_hygro.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(ctr_hygro, LOG_LEVEL_DBG);

static K_MUTEX_DEFINE(m_mut);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(sht30_ext));

int ctr_hygro_read(float *temperature, float *humidity)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	ret = sensor_sample_fetch(m_dev);

	if (ret < 0) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val;

	ret = sensor_channel_get(m_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);

	if (ret < 0) {
		LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float tmp_temperature = sensor_value_to_double(&val);

	LOG_DBG("Temperature: %.2f C", tmp_temperature);

	if (temperature != NULL) {
		*temperature = tmp_temperature;
	}

	ret = sensor_channel_get(m_dev, SENSOR_CHAN_HUMIDITY, &val);

	if (ret < 0) {
		LOG_ERR("Call `sensor_channel_get` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float tmp_humidity = sensor_value_to_double(&val);

	LOG_DBG("Humidity: %.1f %%", tmp_humidity);

	if (humidity != NULL) {
		*humidity = tmp_humidity;
	}

	k_mutex_unlock(&m_mut);
	return 0;
}

static int cmd_hygro_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float temperature;
	float humidity;

	ret = ctr_hygro_read(&temperature, &humidity);

	if (ret < 0) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "temperature: %.2f C", temperature);
	shell_print(shell, "humidity: %.1f %%", humidity);

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_hygro,
                               SHELL_CMD_ARG(read, NULL, "Read sensor data.", cmd_hygro_read, 1, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(hygro, &sub_hygro, "Hygrometer commands.", print_help);
