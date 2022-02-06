/* CHESTER includes */
#include <ctr_therm.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/sensor.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(ctr_therm, LOG_LEVEL_DBG);

static K_MUTEX_DEFINE(m_mut);
static bool m_init;

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(tmp112));

int ctr_therm_init(void)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (m_init) {
		LOG_ERR("Already initialized");
		k_mutex_unlock(&m_mut);
		return -EACCES;
	}

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `TMP112` not ready");
		k_mutex_unlock(&m_mut);
		return -EINVAL;
	}

	struct sensor_value val_scale = {
		.val1 = 128,
		.val2 = 0,
	};
	ret = sensor_attr_set(m_dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_FULL_SCALE, &val_scale);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed (SENSOR_ATTR_FULL_SCALE): %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val_freq = {
		.val1 = 0,
		.val2 = 250000,
	};
	ret = sensor_attr_set(m_dev, SENSOR_CHAN_AMBIENT_TEMP, SENSOR_ATTR_SAMPLING_FREQUENCY,
	                      &val_freq);
	if (ret) {
		LOG_ERR("Call `sensor_attr_set` failed (SENSOR_ATTR_SAMPLING_FREQUENCY): %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	m_init = true;

	k_mutex_unlock(&m_mut);
	return 0;
}

int ctr_therm_read(float *temperature)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	if (!m_init) {
		LOG_ERR("Not initialized");
		k_mutex_unlock(&m_mut);
		return -EACCES;
	}

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device `TMP112` not ready");
		k_mutex_unlock(&m_mut);
		return -EINVAL;
	}

	ret = sensor_sample_fetch(m_dev);
	if (ret) {
		LOG_ERR("Call `sensor_sample_fetch` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	struct sensor_value val;
	ret = sensor_channel_get(m_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (ret) {
		LOG_ERR("Call `sensor_channel_get` failed:t %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	float temperature_ = sensor_value_to_double(&val);

	LOG_DBG("Temperature: %.2f C", temperature_);

	if (temperature != NULL) {
		*temperature = temperature_;
	}

	k_mutex_unlock(&m_mut);
	return 0;
}

static int cmd_therm_init(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = ctr_therm_init();
	if (ret) {
		LOG_ERR("Call `ctr_therm_init` failed: %d", ret);
		shell_error(shell, "command failed");
		return ret;
	}

	return 0;
}

static int cmd_therm_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	float temperature;
	ret = ctr_therm_read(&temperature);
	if (ret) {
		LOG_ERR("Call `ctr_therm_read` failed: %d", ret);
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

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_therm,

	SHELL_CMD_ARG(init, NULL,
	              "Initialize sensor.",
	              cmd_therm_init, 1, 0),

	SHELL_CMD_ARG(read, NULL,
	              "Read sensor data.",
	              cmd_therm_read, 1, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(therm, &sub_therm, "Thermometer commands.", print_help);

/* clang-format on */
