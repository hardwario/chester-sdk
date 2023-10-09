#if defined(CONFIG_APP_LAMBRECHT)

#include "app_lambrecht.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(app_lambrecht, LOG_LEVEL_DBG);

enum address {
	ADDRESS_WIND_SPEED = 1,
	ADDRESS_WIND_DIRECTION = 2,
	ADDRESS_RAINFALL = 3,
	ADDRESS_THP = 4,
	ADDRESS_PYRANOMETER = 247,
};

enum reg {
	REG_TEMPERATURE = 30401,
	REG_HUMIDITY = 30601,
	REG_DEW_POINT = 30701,
	REG_PRESSURE = 30801,
	REG_WIND_SPEED = 30001,
	REG_WIND_SPEED_AVG = 30002,
	REG_WIND_DIRECTION = 30201,
	REG_RAINFALL_TOTAL = 31001,
	REG_RAINFALL_INTENSITY = 31201,
	REG_ILLUMINANCE = 0,
};

static int m_iface;
static const struct device *m_ctr_x2 = DEVICE_DT_GET(DT_NODELABEL(ctr_x2_sc16is740_a));

static int read_reg(enum address addr, enum reg reg, uint16_t *out, size_t quantity)
{
	int ret;

	ret = modbus_read_input_regs(m_iface, addr, reg, out, quantity);
	if (ret) {
		LOG_ERR("Call `modbus_read_input_regs` failed: %d", ret);
		return ret;
	}

	return ret;
}

int app_lambrecht_disable(void)
{
	int ret;

	ret = pm_device_action_run(m_ctr_x2, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_lambrecht_enable(void)
{
	int ret;

	ret = pm_device_action_run(m_ctr_x2, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_lambrecht_init(void)
{
	int ret;

	const char iface_name[] = {
		DEVICE_DT_NAME(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)),
	};

	m_iface = modbus_iface_get_by_name(iface_name);

	const struct modbus_iface_param param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = 500000,
		.serial =
			{
				.baud = 19200,
				.parity = UART_CFG_PARITY_EVEN,
				.stop_bits_client = UART_CFG_STOP_BITS_1,
			},
	};
	ret = modbus_init_client(m_iface, param);
	if (ret) {
		LOG_ERR("Call `modbus_init_client` failed: %d", ret);
		return ret;
	}

	ret = app_lambrecht_disable();
	if (ret) {
		LOG_ERR("Call `app_lambrecht_disable` failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_lambrecht_read_wind_speed(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_WIND_SPEED, REG_WIND_SPEED_AVG, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 10.f;

	return 0;
}

int app_lambrecht_read_wind_direction(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_WIND_DIRECTION, REG_WIND_DIRECTION, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 10.f;

	return 0;
}

int app_lambrecht_read_temperature(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_THP, REG_TEMPERATURE, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 10.f;

	return 0;
}

int app_lambrecht_read_humidity(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_THP, REG_HUMIDITY, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 10.f;

	return 0;
}

int app_lambrecht_read_dew_point(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_THP, REG_DEW_POINT, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 10.f;

	return 0;
}

int app_lambrecht_read_pressure(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_THP, REG_PRESSURE, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 1000.f * 10.f;

	return 0;
}

int app_lambrecht_read_rainfall_total(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_RAINFALL, REG_RAINFALL_TOTAL, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 10.f;

	return 0;
}

int app_lambrecht_read_rainfall_intensity(float *out)
{
	int ret;

	int16_t reg;
	ret = read_reg(ADDRESS_RAINFALL, REG_RAINFALL_INTENSITY, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 1000.f;

	return 0;
}

int app_lambrecht_read_illuminance(float *out)
{
	int ret;

	uint16_t reg;
	ret = read_reg(ADDRESS_PYRANOMETER, REG_ILLUMINANCE, &reg, 1);
	if (ret) {
		LOG_ERR("Call `read_reg` failed: %d", ret);
		return ret;
	}

	*out = reg / 1240.f;

	return 0;
}

#endif /* defined(CONFIG_APP_LAMBRECHT) */
