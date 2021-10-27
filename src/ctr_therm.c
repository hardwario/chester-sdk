#include <ctr_therm.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_therm, LOG_LEVEL_DBG);

static K_MUTEX_DEFINE(m_mut);
static bool m_init;

int hio_therm_init(void)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (m_init) {
		LOG_ERR("Already initialized");
		k_mutex_unlock(&m_mut);
		return -EACCES;
	}

	const struct device *dev = device_get_binding("TMP112");

	if (dev == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	struct sensor_value val;

	val.val1 = 128;
	val.val2 = 0;

	ret = sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_FULL_SCALE, &val);

	if (ret < 0) {
		LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	val.val1 = 0;
	val.val2 = 250000;

	ret = sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_SAMPLING_FREQUENCY, &val);

	if (ret < 0) {
		LOG_ERR("Call `sensor_attr_set` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	m_init = true;

	k_mutex_unlock(&m_mut);
	return 0;
}

int hio_therm_read(float *temperature)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!m_init) {
		LOG_ERR("Not initialized");
		k_mutex_unlock(&m_mut);
		return -EACCES;
	}

	const struct device *dev = device_get_binding("TMP112");

	if (dev == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		k_mutex_unlock(&m_mut);
		return -ENODEV;
	}

	ret = sensor_sample_fetch(dev);

	if (ret < 0) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val;

	ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);

	if (ret < 0) {
		LOG_ERR("Call `sensor_channel_get` failed:t %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float tmp_temperature = sensor_value_to_double(&val);
	;

	LOG_DBG("Temperature: %.2f C", tmp_temperature);

	if (temperature != NULL) {
		*temperature = tmp_temperature;
	}

	k_mutex_unlock(&m_mut);
	return 0;
}

static int cmd_therm_init(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = hio_therm_init();

	if (ret < 0) {
		LOG_ERR("Call `hio_therm_init` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_therm_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float temperature;

	ret = hio_therm_read(&temperature);

	if (ret < 0) {
		LOG_ERR("Call `hio_therm_read` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	shell_print(shell, "temperature: %.2f C", temperature);

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

SHELL_STATIC_SUBCMD_SET_CREATE(sub_therm,
                               SHELL_CMD_ARG(init, NULL, "Initialize sensor.", cmd_therm_init, 1,
                                             0),
                               SHELL_CMD_ARG(read, NULL, "Read sensor data.", cmd_therm_read, 1, 0),
                               SHELL_SUBCMD_SET_END, );

SHELL_CMD_REGISTER(therm, &sub_therm, "Thermometer commands.", print_help);
