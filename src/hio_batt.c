#include <hio_batt.h>
#include <hio_bsp.h>
#include <hio_bus_i2c.h>

/* Zephyr includes */
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_batt, LOG_LEVEL_DBG);

#define ADC_I2C_ADDR 0x4b
#define WAIT_TIME K_SECONDS(1)
#define LOAD_TIME K_SECONDS(9)
#define DIVIDER_R1 10
#define DIVIDER_R2 5
#define LOAD_R 100

static K_MUTEX_DEFINE(m_mut);

static int measure(uint16_t *voltage)
{
	int ret;

	struct hio_bus_i2c *i2c = hio_bsp_get_i2c();

	ret = hio_bus_i2c_mem_write_16b(i2c, ADC_I2C_ADDR, 0x01, 0xc583);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_write_16b` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(10));

	int16_t result;

	ret = hio_bus_i2c_mem_read_16b(i2c, ADC_I2C_ADDR, 0x00, &result);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_read_16b` failed: %d", ret);
		return ret;
	}

	if (result == 0x7ff0 || result == 0x8000) {
		return -ERANGE;
	}

	if (result < 0) {
		*voltage = 0;

	} else {
		*voltage = (uint32_t)(result >> 4) * 2048 * (DIVIDER_R1 + DIVIDER_R2) /
		           (2047 * DIVIDER_R2);
	}

	return 0;
}

int hio_batt_measure(struct hio_batt_result *result)
{
	int ret;

	k_mutex_lock(&m_mut, K_FOREVER);

	ret = hio_bsp_set_batt_load(false);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_load` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	ret = hio_bsp_set_batt_test(true);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_test` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_sleep(WAIT_TIME);

	ret = measure(&result->voltage_rest_mv);

	if (ret < 0) {
		LOG_ERR("Call `measure` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	LOG_INF("Battery voltage (rest) = %u mV", result->voltage_rest_mv);

	ret = hio_bsp_set_batt_load(true);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_load` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	k_sleep(LOAD_TIME);

	ret = measure(&result->voltage_load_mv);

	if (ret < 0) {
		LOG_ERR("Call `measure` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	LOG_INF("Battery voltage (load) = %u mV", result->voltage_load_mv);

	ret = hio_bsp_set_batt_load(false);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_load` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	ret = hio_bsp_set_batt_test(false);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_test` failed: %d", ret);
		k_mutex_unlock(&m_mut);
		return ret;
	}

	result->current_load_ma = result->voltage_load_mv / LOAD_R;

	LOG_INF("Battery current (load) = %u mA", result->current_load_ma);

	k_mutex_unlock(&m_mut);
	return 0;
}

static int cmd_batt_measure(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	struct hio_batt_result result;

	ret = hio_batt_measure(&result);

	if (ret < 0) {
		LOG_ERR("Call `hio_batt_measure` failed: %d", ret);
		return ret;
	}

	shell_print(shell, "voltage (rest): %u millivolts", result.voltage_rest_mv);
	shell_print(shell, "voltage (load): %u millivolts", result.voltage_load_mv);
	shell_print(shell, "current (load): %u milliamps", result.current_load_ma);

	return 0;
}

static int cmd_batt_load(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = hio_bsp_set_batt_load(true);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_load` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_batt_unload(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	ret = hio_bsp_set_batt_load(false);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_batt_load` failed: %d", ret);
		return ret;
	}

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

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_batt, SHELL_CMD_ARG(measure, NULL, "Measure battery.", cmd_batt_measure, 1, 0),
        SHELL_CMD_ARG(load, NULL, "Load battery.", cmd_batt_load, 1, 0),
        SHELL_CMD_ARG(unload, NULL, "Unload battery.", cmd_batt_unload, 1, 0),
        SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(batt, &sub_batt, "Battery commands.", print_help);
