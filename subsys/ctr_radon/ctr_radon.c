#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>

#include <chester/ctr_radon.h>

#include "ctr_radon_config.h"

LOG_MODULE_REGISTER(ctr_radon, CONFIG_CTR_RADON_LOG_LEVEL);

enum reg {
	REG_CONCENTRATION_TIME = 1,
	REG_CONCENTRATION_L = 2,
	REG_CONCENTRATION_H = 3,
	REG_TEMPERATURE = 4,
	REG_HUMIDITY = 5,
	REG_CONCENTRATION_DAY_L = 17,
	REG_CONCENTRATION_DAY_H = 18,
	REG_RECORD_TIME = 19,
	REG_RECORD_COUNT = 20,
	REG_SPECTRUM_TIME = 21,
	REG_SPECTRUM_COUNT = 22,
	REG_IMPULSES_TOTAL_L = 23,
	REG_IMPULSES_TOTAL_H = 24,
	REG_LIMIT = 33,
	REG_RECORD_INTERVAL = 36,
	REG_SPECTRUM_INTERVAL = 37,
	REG_ALGORITHM = 38,
	REG_REAL_TIME_L = 39,
	REG_REAL_TIME_H = 40,
	REG_CUSTOMER_TEXT = 41,
	REG_DEVICE = 60,
	REG_VERSION_SW = 65,
	REG_SERIAL_NUMBER = 70,
};

static int m_iface;
#if defined(CONFIG_CTR_RADON_SLOT_A)
static const struct device *m_ctr_x2 = DEVICE_DT_GET(DT_NODELABEL(ctr_x2_sc16is740_a));
#elif defined(CONFIG_CTR_RADON_SLOT_B)
static const struct device *m_ctr_x2 = DEVICE_DT_GET(DT_NODELABEL(ctr_x2_sc16is740_b));
#endif

static int read_reg(int addr, enum reg reg, uint16_t *out, size_t quantity)
{
	int ret;

	ret = modbus_read_holding_regs(m_iface, addr, reg, out, quantity);
	if (ret) {
		LOG_ERR("Call `modbus_read_input_regs` failed: %d", ret);
		return ret;
	}

	return ret;
}

int ctr_radon_disable(void)
{
	int ret;

	ret = pm_device_action_run(m_ctr_x2, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_radon_enable(void)
{
	int ret;

	ret = pm_device_action_run(m_ctr_x2, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_radon_read_temperature(int16_t *out)
{
	int ret;

	ret = read_reg(g_ctr_radon_config.modbus_addr, REG_TEMPERATURE, out, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	if (*out > 127) {
		*out -= 256;
	}

	return 0;
}

int ctr_radon_read_humidity(uint16_t *out)
{
	int ret;

	ret = read_reg(g_ctr_radon_config.modbus_addr, REG_HUMIDITY, out, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_radon_read_concentration(uint32_t *out, bool daily)
{
	uint16_t data[2];

	int ret = read_reg(g_ctr_radon_config.modbus_addr,
			   daily ? REG_CONCENTRATION_DAY_L : REG_CONCENTRATION_L, data, 2);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = 0;
	*out |= data[0];
	*out |= data[1] << 16;

	return 0;
}

static int init(void)
{
	int ret;

	ret = ctr_radon_config_init();
	if (ret) {
		LOG_ERR("Call `ctr_radon_config_init` failed: %d", ret);
		return ret;
	}

	m_iface = modbus_iface_get_by_name(
		DEVICE_DT_NAME(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)));

	const struct modbus_iface_param param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = 500000,
		.serial =
			{
				.baud = g_ctr_radon_config.modbus_baud,
				.parity = g_ctr_radon_config.modbus_parity,
				.stop_bits_client = g_ctr_radon_config.modbus_stop_bits,
			},
	};

	ret = modbus_init_client(m_iface, param);
	if (ret) {
		LOG_ERR("Call `modbus_init_client` failed: %d", ret);
		return ret;
	}

	ret = ctr_radon_disable();
	if (ret) {
		LOG_ERR("Call `app_radon_disable` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);