#include <device.h>
#include <drivers/ctr_batt.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_DECLARE(ctr_batt, CONFIG_CTR_BATT_LOG_LEVEL);

static int cmd_rest(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	const struct device *dev = DEVICE_DT_GET_ANY(hardwario_ctr_batt);
	if (!dev) {
		LOG_ERR("No device not found");
		return -ENODEV;
	}

	int rest_mv;
	ret = ctr_batt_get_rest_voltage_mv(dev, &rest_mv, K_MSEC(CTR_BATT_REST_TIMEOUT_DEFAULT_MS));
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_rest_voltage` failed: %d", ret);
		return ret;
	}

	shell_print(shell, "Battery rest voltage = %u mV", rest_mv);

	return 0;
}

static int cmd_load(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	const struct device *dev = DEVICE_DT_GET_ANY(hardwario_ctr_batt);
	if (!dev) {
		LOG_ERR("No device not found");
		return -ENODEV;
	}

	int load_mv;
	ret = ctr_batt_get_load_voltage_mv(dev, &load_mv, K_MSEC(CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS));
	if (ret) {
		LOG_ERR("Call `ctr_batt_get_load_voltage` failed: %d", ret);
		return ret;
	}

	int load_ma;
	ctr_batt_get_load_current_ma(dev, &load_ma, load_mv);

	shell_print(shell, "Battery load voltage = %u mV", load_mv);
	shell_print(shell, "Battery load current = %u mA", load_ma);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_batt,
                               SHELL_CMD_ARG(rest, NULL, "Measure battery voltage", cmd_rest, 0, 0),
                               SHELL_CMD_ARG(load, NULL, "Measure battery voltage under load",
                                             cmd_load, 0, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(batt, &sub_batt, "Battery commands.", NULL);
