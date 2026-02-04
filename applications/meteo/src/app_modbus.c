/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#if defined(FEATURE_HARDWARE_CHESTER_METEO_M)
#include "app_modbus.h"
#include "app_config.h"
#include "app_data.h"

/* CHESTER includes */
#include <chester/ctr_rtc.h>
#include <chester/application/ctr_data.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

LOG_MODULE_REGISTER(app_modbus, LOG_LEVEL_DBG);

static int m_iface = 0;

static const struct device *m_ctr_x2 = NULL;

static int modbus_sample_sensecap(struct app_data_sensecap *device);
static int modbus_sample_cubic_pm(struct app_data_cubic_pm *device);

int app_modbus_init(void)
{
	int ret;

	m_iface = modbus_iface_get_by_name(
		DEVICE_DT_NAME(DT_COMPAT_GET_ANY_STATUS_OKAY(zephyr_modbus_serial)));
	if (m_iface < 0) {
		LOG_ERR("Modbus interface not found. Make sure it's enabled in device tree.");
		return m_iface;
	}

	const struct modbus_iface_param param = {
		.mode = MODBUS_MODE_RTU,
		.rx_timeout = 1000000,
		.serial =
			{
				.baud = g_app_config.modbus_baud,
				.parity = (enum uart_config_parity)g_app_config.modbus_parity,
				.stop_bits_client =
					(enum uart_config_stop_bits)g_app_config.modbus_stop_bits,
			},
	};

	ret = modbus_init_client(m_iface, param);
	if (ret) {
		LOG_ERR("Call `modbus_init_client` failed with error: %d", ret);
		return ret;
	}

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Call `app_modbus_enable` failed: %d", ret);
		return ret;
	}

	LOG_INF("Modbus client initialized on interface %d with baud rate %d, parity %d, stop bits "
		"%d.",
		m_iface, g_app_config.modbus_baud, g_app_config.modbus_parity,
		g_app_config.modbus_stop_bits);

	ret = app_modbus_disable();
	if (ret) {
		LOG_ERR("Call `app_modbus_disable` failed: %d", ret);
		return ret;
	}

	return 0;
}
static int read_reg_ui16(uint8_t slave_addr, uint16_t reg_addr, uint16_t *data)
{
	uint16_t reg_data;
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, &reg_data, 1);
	if (ret) {
		LOG_WRN("Read 0x%04x from %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	*data = reg_data;
	return 0;
}

static int write_reg_ui16(uint8_t slave_addr, uint16_t reg_addr, uint16_t data)
{
	int ret;

	ret = modbus_write_holding_reg(m_iface, slave_addr, reg_addr, data);
	if (ret) {
		LOG_WRN("Write reg 0x%04x to %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	LOG_INF("Successfully wrote 0x%04x to reg 0x%04x at slave %d", data, reg_addr, slave_addr);
	return 0;
}
static int read_reg_s32(uint8_t slave_addr, uint16_t reg_addr, int32_t *data)
{
	uint16_t reg_data[2];
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, reg_data, 2);
	if (ret) {
		LOG_WRN("Read 0x%04x (32-bit) from %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	*data = (int32_t)reg_data[0] << 16 | reg_data[1];
	return 0;
}

static int read_reg_u32(uint8_t slave_addr, uint16_t reg_addr, uint32_t *data)
{
	uint16_t reg_data[2];
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, reg_data, 2);
	if (ret) {
		LOG_WRN("Read 0x%04x (32-bit) from %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	*data = (uint32_t)reg_data[0] << 16 | reg_data[1];
	return 0;
}

#if defined(FEATURE_CHESTER_APP_LAMBRECHT)
#error "FEATURE_CHESTER_APP_LAMBRECHT is deprecated, use CONFIG_FEATURE_CHESTER_APP_LAMBRECHT instead"
static int read_reg_s16(uint8_t slave_addr, uint16_t reg_addr, int16_t *data)
{
	uint16_t reg_data;
	int ret = modbus_read_input_regs(m_iface, slave_addr, reg_addr, &reg_data, 1);
	if (ret) {
		LOG_WRN("Read 0x%04x from %d failed: %d", reg_addr, slave_addr, ret);
		return ret;
	}

	*data = (int16_t)reg_data;
	return 0;
}

/* Lambrecht sensor slave addresses */
enum lambrecht_address {
	LAMBRECHT_ADDRESS_WIND_SPEED = 1,
	LAMBRECHT_ADDRESS_WIND_DIRECTION = 2,
	LAMBRECHT_ADDRESS_RAINFALL = 3,
	LAMBRECHT_ADDRESS_THP = 4,
	LAMBRECHT_ADDRESS_PYRANOMETER = 247,
};

/* Lambrecht register addresses (converted from Modbus input register numbers) */
enum lambrecht_reg {
	LAMBRECHT_REG_TEMPERATURE = 30401 - 30001,        /* 400 */
	LAMBRECHT_REG_HUMIDITY = 30601 - 30001,           /* 600 */
	LAMBRECHT_REG_DEW_POINT = 30701 - 30001,          /* 700 */
	LAMBRECHT_REG_PRESSURE = 30801 - 30001,           /* 800 */
	LAMBRECHT_REG_WIND_SPEED = 30001 - 30001,         /* 0 */
	LAMBRECHT_REG_WIND_SPEED_AVG = 30002 - 30001,     /* 1 */
	LAMBRECHT_REG_WIND_DIRECTION = 30201 - 30001,     /* 200 */
	LAMBRECHT_REG_RAINFALL_TOTAL = 31001 - 30001,     /* 1000 */
	LAMBRECHT_REG_RAINFALL_INTENSITY = 31201 - 30001, /* 1200 */
	LAMBRECHT_REG_ILLUMINANCE = 0,
};

static int modbus_sample_lambrecht(struct app_data_lambrecht *device)
{
	int ret;
	int16_t modbus_data_s16;
	uint16_t modbus_data_u16;
	int global_ret = 0;

	LOG_DBG("Sampling Lambrecht meteo station");

	if (device->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Samples full");
		return -ENOSPC;
	}

	/* Wind speed average (slave 1, reg 1) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_WIND_SPEED, LAMBRECHT_REG_WIND_SPEED_AVG,
			   &modbus_data_s16);
	float wind_speed_sample = (ret == 0) ? modbus_data_s16 / 10.f : NAN;
	device->wind_speed_samples[device->sample_count] = wind_speed_sample;
	LOG_INF("Lambrecht Wind Speed: %.1f m/s", (double)wind_speed_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Wind direction (slave 2, reg 200) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_WIND_DIRECTION, LAMBRECHT_REG_WIND_DIRECTION,
			   &modbus_data_s16);
	float wind_direction_sample = (ret == 0) ? modbus_data_s16 / 10.f : NAN;
	device->wind_direction_samples[device->sample_count] = wind_direction_sample;
	LOG_INF("Lambrecht Wind Direction: %.1f deg", (double)wind_direction_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Temperature (slave 4, reg 400) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_THP, LAMBRECHT_REG_TEMPERATURE, &modbus_data_s16);
	float temperature_sample = (ret == 0) ? modbus_data_s16 / 10.f : NAN;
	device->temperature_samples[device->sample_count] = temperature_sample;
	LOG_INF("Lambrecht Temperature: %.1f C", (double)temperature_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Humidity (slave 4, reg 600) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_THP, LAMBRECHT_REG_HUMIDITY, &modbus_data_s16);
	float humidity_sample = (ret == 0) ? modbus_data_s16 / 10.f : NAN;
	device->humidity_samples[device->sample_count] = humidity_sample;
	LOG_INF("Lambrecht Humidity: %.1f %%", (double)humidity_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Dew point (slave 4, reg 700) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_THP, LAMBRECHT_REG_DEW_POINT, &modbus_data_s16);
	float dew_point_sample = (ret == 0) ? modbus_data_s16 / 10.f : NAN;
	device->dew_point_samples[device->sample_count] = dew_point_sample;
	LOG_INF("Lambrecht Dew Point: %.1f C", (double)dew_point_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Pressure (slave 4, reg 800) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_THP, LAMBRECHT_REG_PRESSURE, &modbus_data_s16);
	float pressure_sample = (ret == 0) ? modbus_data_s16 / 1000.f * 10.f : NAN;
	device->pressure_samples[device->sample_count] = pressure_sample;
	LOG_INF("Lambrecht Pressure: %.1f hPa", (double)pressure_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Rainfall total (slave 3, reg 1000) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_RAINFALL, LAMBRECHT_REG_RAINFALL_TOTAL,
			   &modbus_data_s16);
	float rainfall_total_sample = (ret == 0) ? modbus_data_s16 / 10.f : NAN;
	device->rainfall_total_samples[device->sample_count] = rainfall_total_sample;
	LOG_INF("Lambrecht Rainfall Total: %.1f mm", (double)rainfall_total_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Rainfall intensity (slave 3, reg 1200) */
	ret = read_reg_s16(LAMBRECHT_ADDRESS_RAINFALL, LAMBRECHT_REG_RAINFALL_INTENSITY,
			   &modbus_data_s16);
	float rainfall_intensity_sample = (ret == 0) ? modbus_data_s16 / 1000.f : NAN;
	device->rainfall_intensity_samples[device->sample_count] = rainfall_intensity_sample;
	LOG_INF("Lambrecht Rainfall Intensity: %.3f mm/min", (double)rainfall_intensity_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* Illuminance (slave 247, reg 0) */
	ret = read_reg_ui16(LAMBRECHT_ADDRESS_PYRANOMETER, LAMBRECHT_REG_ILLUMINANCE,
			    &modbus_data_u16);
	float illuminance_sample = (ret == 0) ? modbus_data_u16 / 1240.f : NAN;
	device->illuminance_samples[device->sample_count] = illuminance_sample;
	LOG_INF("Lambrecht Illuminance: %.3f lux", (double)illuminance_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	if (global_ret == 0) {
		device->sample_count++;
	}

	return global_ret;
}

static int modbus_aggreg_lambrecht(struct app_data_lambrecht *device)
{
	int ret;

	if (!device->measurement_count) {
		ret = ctr_rtc_get_ts(&device->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}
	}

	if (device->measurement_count >= APP_DATA_MAX_MEASUREMENTS) {
		LOG_WRN("Measurement full");
		return -ENOSPC;
	}

	aggreg_sample(device->wind_speed_samples, device->sample_count,
		      &device->wind_speed_measurements[device->measurement_count]);
	aggreg_sample(device->wind_direction_samples, device->sample_count,
		      &device->wind_direction_measurements[device->measurement_count]);
	aggreg_sample(device->temperature_samples, device->sample_count,
		      &device->temperature_measurements[device->measurement_count]);
	aggreg_sample(device->humidity_samples, device->sample_count,
		      &device->humidity_measurements[device->measurement_count]);
	aggreg_sample(device->dew_point_samples, device->sample_count,
		      &device->dew_point_measurements[device->measurement_count]);
	aggreg_sample(device->pressure_samples, device->sample_count,
		      &device->pressure_measurements[device->measurement_count]);
	aggreg_sample(device->rainfall_total_samples, device->sample_count,
		      &device->rainfall_total_measurements[device->measurement_count]);
	aggreg_sample(device->rainfall_intensity_samples, device->sample_count,
		      &device->rainfall_intensity_measurements[device->measurement_count]);
	aggreg_sample(device->illuminance_samples, device->sample_count,
		      &device->illuminance_measurements[device->measurement_count]);

	/* Calculate rainfall total sum */
	for (int i = 0; i < device->sample_count; i++) {
		device->rainfall_total_sum += device->rainfall_total_samples[i];
	}

	device->measurement_count++;
	device->sample_count = 0;

	LOG_INF("Measurement count: %d", device->measurement_count);

	return 0;
}

#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

int app_modbus_enable(void)
{
	int ret;

	if (m_ctr_x2 == NULL) {
		m_ctr_x2 = DEVICE_DT_GET(DT_NODELABEL(ctr_x2_sc16is740_a));
		if (!device_is_ready(m_ctr_x2)) {
			LOG_ERR("Modbus device (SC16IS740) not ready");
			return -ENODEV;
		}
	}

	ret = pm_device_action_run(m_ctr_x2, PM_DEVICE_ACTION_RESUME);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}
	return 0;
}

int app_modbus_disable(void)
{
	int ret;

	if (m_ctr_x2 == NULL || !device_is_ready(m_ctr_x2)) {
		return 0;
	}

	ret = pm_device_action_run(m_ctr_x2, PM_DEVICE_ACTION_SUSPEND);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int modbus_sample_sensecap(struct app_data_sensecap *device)
{
	int ret;
	int32_t modbus_data_s32;
	int global_ret = 0;
	uint8_t slave_addr = g_app_config.meteo_addr;
	const float divisor = 1000.0f;

	LOG_DBG("Sampling SenseCAP at addr %d", slave_addr);

	if (device->sample_count >= APP_DATA_MAX_SAMPLES) {
		LOG_WRN("Samples full");
		return -ENOSPC;
	}

	// Air temperature (0x0000)
	ret = read_reg_s32(slave_addr, 0x0000, &modbus_data_s32);
	float air_temperature_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
	device->air_temperature_samples[device->sample_count] = air_temperature_sample;
	LOG_INF("SenseCAP Air Temp: %f C (Reg 0x0000)", (double)air_temperature_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	// Air humidity (0x0002)
	ret = read_reg_s32(slave_addr, 0x0002, &modbus_data_s32);
	float air_humidity_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
	device->air_humidity_samples[device->sample_count] = air_humidity_sample;
	LOG_INF("SenseCAP Air Humidity: %f %% (Reg 0x0002)", (double)air_humidity_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	// Barometric pressure (0x0004)
	ret = read_reg_s32(slave_addr, 0x0004, &modbus_data_s32);
	float barometric_pressure_sample;
	if (ret == 0) {
		barometric_pressure_sample = (float)modbus_data_s32 / divisor;
	} else {
		barometric_pressure_sample = NAN;
		global_ret = (global_ret == 0) ? ret : global_ret;
	}
	device->barometric_pressure_samples[device->sample_count] = barometric_pressure_sample;
	LOG_INF("SenseCAP Baro Pressure: %f Pa (Reg 0x0004)", (double)barometric_pressure_sample);

	// Common Wind Data (Average Wind Direction 0x000C, Average Wind Speed 0x0012)
	ret = read_reg_s32(slave_addr, 0x000C, &modbus_data_s32);
	float wind_direction_avg_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
	device->wind_direction_avg_samples[device->sample_count] = wind_direction_avg_sample;
	LOG_INF("SenseCAP Wind Dir Avg: %f deg (Reg 0x000C)", (double)wind_direction_avg_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	ret = read_reg_s32(slave_addr, 0x0012, &modbus_data_s32);
	float wind_speed_avg_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
	device->wind_speed_avg_samples[device->sample_count] = wind_speed_avg_sample;
	LOG_INF("SenseCAP Wind Speed Avg: %f m/s (Reg 0x0012)", (double)wind_speed_avg_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	//  S1000 SPECIFIC (Light Intensity, Rainfall, PM2.5, PM10, CO2)
	if (g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000) {
		// Light intensity (0x0006)
		ret = read_reg_s32(slave_addr, 0x0006, &modbus_data_s32);
		float light_intensity_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
		device->light_intensity_samples[device->sample_count] = light_intensity_sample;
		LOG_INF("SenseCAP Light Intensity: %f lux (Reg 0x0006)",
			(double)light_intensity_sample);
		global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

		// Accumulated rainfall (0x0014)
		ret = read_reg_s32(slave_addr, 0x0014, &modbus_data_s32);
		float accumulated_rainfall_sample =
			(ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
		device->accumulated_rainfall_samples[device->sample_count] =
			accumulated_rainfall_sample;
		LOG_INF("SenseCAP Rainfall Acc: %f mm (Reg 0x0014)",
			(double)accumulated_rainfall_sample);
		global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

		// PM2.5 (0x0030)
		ret = read_reg_s32(slave_addr, 0x0030, &modbus_data_s32);
		float pm2_5_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
		device->pm2_5_samples[device->sample_count] = pm2_5_sample;
		LOG_INF("SenseCAP PM2.5: %f ug/m3 (Reg 0x0030)", (double)pm2_5_sample);
		global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

		// PM10 (0x0032)
		ret = read_reg_s32(slave_addr, 0x0032, &modbus_data_s32);
		float pm10_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
		device->pm10_samples[device->sample_count] = pm10_sample;
		LOG_INF("SenseCAP PM10: %f ug/m3 (Reg 0x0032)", (double)pm10_sample);
		global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

		// CO2 (0x0040)
		ret = read_reg_s32(slave_addr, 0x0040, &modbus_data_s32);
		float co2_sample = (ret == 0) ? (float)modbus_data_s32 / divisor : NAN;
		device->co2_samples[device->sample_count] = co2_sample;
		LOG_INF("SenseCAP CO2: %f ppm (Reg 0x0040)", (double)co2_sample);
		global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;
	}

	if (global_ret == 0) {
		device->sample_count++;
	}

	return global_ret;
}

static int modbus_sample_cubic_pm(struct app_data_cubic_pm *device)
{
	int ret;
	uint32_t modbus_data_u32;
	uint16_t modbus_data_u16;
	int global_ret = 0;
	uint8_t slave_addr = g_app_config.pm_addr;

	LOG_DBG("Sampling Cubic OPM PM Sensor at addr %d", slave_addr);

	/* Version (IR1, addr 0) - 16bit, divide by 100*/
	ret = read_reg_ui16(slave_addr, 0, &modbus_data_u16);
	float version_sample = (ret == 0) ? (float)modbus_data_u16 / 100.0f : NAN;
	device->version_samples[device->sample_count] = version_sample;
	LOG_INF("Cubic PM Version: %.2f (Reg 0)", (double)version_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* TSP (IR6/7, addr 5) - 32bit */
	ret = read_reg_u32(slave_addr, 5, &modbus_data_u32);
	float tsp_sample = (ret == 0) ? (float)modbus_data_u32 : NAN;
	device->tsp_samples[device->sample_count] = tsp_sample;
	LOG_INF("Cubic PM TSP: %.1f ug/m3 (Reg 5+6)", (double)tsp_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* PM1.0 (IR8/9, addr 7) - 32bit */
	ret = read_reg_u32(slave_addr, 7, &modbus_data_u32);
	float pm1_0_sample = (ret == 0) ? (float)modbus_data_u32 : NAN;
	device->pm1_0_samples[device->sample_count] = pm1_0_sample;
	LOG_INF("Cubic PM PM1.0: %.1f ug/m3 (Reg 7+8)", (double)pm1_0_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* PM2.5 (IR10/11, addr 9) - 32bit */
	ret = read_reg_u32(slave_addr, 9, &modbus_data_u32);
	float pm2_5_sample = (ret == 0) ? (float)modbus_data_u32 : NAN;
	device->pm2_5_samples[device->sample_count] = pm2_5_sample;
	LOG_INF("Cubic PM PM2.5: %.1f ug/m3 (Reg 9+10)", (double)pm2_5_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	/* PM10 (IR14/15, addr 13) - 32bit */
	ret = read_reg_u32(slave_addr, 13, &modbus_data_u32);
	float pm10_sample = (ret == 0) ? (float)modbus_data_u32 : NAN;
	device->pm10_samples[device->sample_count] = pm10_sample;
	LOG_INF("Cubic PM PM10: %.1f ug/m3 (Reg 13+14)", (double)pm10_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	ret = read_reg_ui16(slave_addr, 23, &modbus_data_u16);
	/* Note: "multiplied by 100" in documentation */
	float gas_flow_sample = (ret == 0) ? (float)modbus_data_u16 / 100.0f : NAN;
	device->gas_flow_samples[device->sample_count] = gas_flow_sample;
	LOG_INF("Cubic PM Gas Flow: %.2f (Reg 23)", (double)gas_flow_sample);
	global_ret = (ret != 0 && global_ret == 0) ? ret : global_ret;

	if (global_ret == 0) {
		device->sample_count++;
	}

	return global_ret;
}

static void aggreg_sample(float *samples, size_t count, struct ctr_data_aggreg *aggreg)
{
	float min = NAN;
	float max = NAN;
	float avg = NAN;
	float mdn = NAN;

	if (!count) {
		goto out;
	}

	for (size_t i = 0; i < count; i++) {
		if (isnan(samples[i])) {
			goto out;
		}
	}

	float sorted[APP_DATA_MAX_SAMPLES];
	memcpy(sorted, samples, count * sizeof(float));

	for (size_t i = 0; i < count - 1; i++) {
		for (size_t j = 0; j < count - i - 1; j++) {
			if (sorted[j] > sorted[j + 1]) {
				float temp = sorted[j];
				sorted[j] = sorted[j + 1];
				sorted[j + 1] = temp;
			}
		}
	}

	min = sorted[0];
	max = sorted[count - 1];

	double sum = 0;
	for (size_t i = 0; i < count; i++) {
		sum += (double)samples[i];
	}
	avg = sum / count;

	mdn = sorted[count / 2];

out:
	aggreg->min = min;
	aggreg->max = max;
	aggreg->avg = avg;
	aggreg->mdn = mdn;
}

static int modbus_aggreg_sensecap(struct app_data_sensecap *device)
{
	int ret;

	if (!device->measurement_count) {
		ret = ctr_rtc_get_ts(&device->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}
	}

	if (device->measurement_count >= APP_DATA_MAX_MEASUREMENTS) {
		LOG_WRN("Measurement full");
		return -ENOSPC;
	}

	aggreg_sample(device->air_temperature_samples, device->sample_count,
		      &device->air_temperature_measurements[device->measurement_count]);
	aggreg_sample(device->air_humidity_samples, device->sample_count,
		      &device->air_humidity_measurements[device->measurement_count]);
	aggreg_sample(device->barometric_pressure_samples, device->sample_count,
		      &device->barometric_pressure_measurements[device->measurement_count]);
	aggreg_sample(device->wind_direction_avg_samples, device->sample_count,
		      &device->wind_direction_avg_measurements[device->measurement_count]);
	aggreg_sample(device->wind_speed_avg_samples, device->sample_count,
		      &device->wind_speed_avg_measurements[device->measurement_count]);

	if (g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000) {
		aggreg_sample(device->light_intensity_samples, device->sample_count,
			      &device->light_intensity_measurements[device->measurement_count]);
		aggreg_sample(
			device->accumulated_rainfall_samples, device->sample_count,
			&device->accumulated_rainfall_measurements[device->measurement_count]);
		aggreg_sample(device->pm2_5_samples, device->sample_count,
			      &device->pm2_5_measurements[device->measurement_count]);
		aggreg_sample(device->pm10_samples, device->sample_count,
			      &device->pm10_measurements[device->measurement_count]);
		aggreg_sample(device->co2_samples, device->sample_count,
			      &device->co2_measurements[device->measurement_count]);
	}

	device->measurement_count++;
	device->sample_count = 0;

	LOG_INF("Measurement count: %d", device->measurement_count);

	return 0;
}

static int modbus_aggreg_cubic_pm(struct app_data_cubic_pm *device)
{
	int ret;

	if (!device->measurement_count) {
		ret = ctr_rtc_get_ts(&device->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			return ret;
		}
	}

	if (device->measurement_count >= APP_DATA_MAX_MEASUREMENTS) {
		LOG_WRN("Measurement full");
		return -ENOSPC;
	}

	aggreg_sample(device->version_samples, device->sample_count,
		      &device->version_measurements[device->measurement_count]);
	aggreg_sample(device->tsp_samples, device->sample_count,
		      &device->tsp_measurements[device->measurement_count]);
	aggreg_sample(device->pm1_0_samples, device->sample_count,
		      &device->pm1_0_measurements[device->measurement_count]);
	aggreg_sample(device->pm2_5_samples, device->sample_count,
		      &device->pm2_5_measurements[device->measurement_count]);
	aggreg_sample(device->pm10_samples, device->sample_count,
		      &device->pm10_measurements[device->measurement_count]);
	aggreg_sample(device->gas_flow_samples, device->sample_count,
		      &device->gas_flow_measurements[device->measurement_count]);

	device->measurement_count++;
	device->sample_count = 0;

	LOG_INF("Measurement count: %d", device->measurement_count);

	return 0;
}

int app_modbus_aggreg(void)
{
	int ret = 0;

	app_data_lock();

	if ((g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000) ||
	    (g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S500)) {
		ret |= modbus_aggreg_sensecap(&g_app_data.sensecap);
	}

	if (g_app_config.pm_type == APP_CONFIG_PM_CUBIC_6303) {
		ret |= modbus_aggreg_cubic_pm(&g_app_data.cubic_pm);
	}

#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)
	if (g_app_config.meteo_type == APP_CONFIG_METEO_LAMBRECHT) {
		ret |= modbus_aggreg_lambrecht(&g_app_data.lambrecht);
	}
#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

	app_data_unlock();

	return ret;
}

int app_modbus_clear(void)
{
	app_data_lock();

	g_app_data.sensecap.measurement_count = 0;
	g_app_data.cubic_pm.measurement_count = 0;

#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)
	g_app_data.lambrecht.measurement_count = 0;
	g_app_data.lambrecht.rainfall_total_sum = 0;
#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

	app_data_unlock();

	return 0;
}

int app_modbus_sample(void)
{
	int ret = 0;

	ret = app_modbus_enable();
	if (ret) {
		LOG_ERR("Call `app_modbus_enable` failed: %d", ret);
		return ret;
	}

	app_data_lock();

	if ((g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000) ||
	    (g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S500)) {
		ret |= modbus_sample_sensecap(&g_app_data.sensecap);
	} else {
		memset(&g_app_data.sensecap, 0, sizeof(g_app_data.sensecap));
	}

	if (g_app_config.pm_type == APP_CONFIG_PM_CUBIC_6303) {
		ret |= modbus_sample_cubic_pm(&g_app_data.cubic_pm);
	} else {
		memset(&g_app_data.cubic_pm, 0, sizeof(g_app_data.cubic_pm));
	}

#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)
	if (g_app_config.meteo_type == APP_CONFIG_METEO_LAMBRECHT) {
		ret |= modbus_sample_lambrecht(&g_app_data.lambrecht);
	} else {
		memset(&g_app_data.lambrecht, 0, sizeof(g_app_data.lambrecht));
	}
#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

	app_data_unlock();

	int disable_ret = app_modbus_disable();
	if (disable_ret) {
		LOG_ERR("Call `app_modbus_disable` failed: %d", disable_ret);
		return disable_ret;
	}

	return ret;
}
int app_modbus_set_sensecap_addr(uint8_t current_addr, uint8_t new_addr)
{

	const uint16_t reg_addr = 0x1000;
	int ret;

	LOG_INF("Attempting to change SenseCAP address from %d to %d...", current_addr, new_addr);

	app_modbus_enable();
	ret = write_reg_ui16(current_addr, reg_addr, (uint16_t)new_addr);
	app_modbus_disable();

	if (ret == 0) {
		LOG_INF("Success! SenseCAP address *should* now be %d.", new_addr);
		LOG_WRN("Please restart (disconnect and reconnect) the SenseCAP device to apply "
			"the new address.");
	}
	return ret;
}

int app_modbus_set_cubic_pm_addr(uint8_t current_addr, uint8_t new_addr)
{

	const uint16_t reg_addr = 2;
	int ret;

	LOG_INF("Attempting to change Cubic PM address from %d to %d...", current_addr, new_addr);

	app_modbus_enable();
	ret = write_reg_ui16(current_addr, reg_addr, (uint16_t)new_addr);
	app_modbus_disable();

	if (ret == 0) {
		LOG_INF("Success! Cubic PM address *should* now be %d.", new_addr);
	}
	return ret;
}
/* Helper function for parsing Modbus address arguments */
static int parse_addr_arg(const struct shell *shell, char *arg, uint8_t *addr)
{
	char *endptr;
	long val = strtol(arg, &endptr, 10);

	if (*endptr != '\0' || val < 1 || val > 247) {
		shell_error(shell, "Invalid address '%s'. Must be 1-247.", arg);
		return -EINVAL;
	}
	*addr = (uint8_t)val;
	return 0;
}

int cmd_modbus_set_sensecap(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(shell, "Usage: %s <current_addr> <new_addr>", argv[0]);
		return -EINVAL;
	}

	uint8_t current_addr, new_addr;
	if (parse_addr_arg(shell, argv[1], &current_addr) != 0) {
		return -EINVAL;
	}
	if (parse_addr_arg(shell, argv[2], &new_addr) != 0) {
		return -EINVAL;
	}

	shell_print(shell, "Setting SenseCAP address...");
	shell_warn(shell, "WARNING: Ensure ONLY the SenseCAP is connected and is at address %d.",
		   current_addr);

	int ret = app_modbus_set_sensecap_addr(current_addr, new_addr);
	if (ret) {
		shell_error(shell, "Failed to set address. Error: %d", ret);
	} else {
		shell_print(shell, "Command sent successfully. Target address is %d.", new_addr);
		shell_print(shell, "Please power-cycle the SenseCAP sensor to apply the change.");
	}
	return ret;
}

int cmd_modbus_set_cubic(const struct shell *shell, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(shell, "Usage: %s <current_addr> <new_addr>", argv[0]);
		return -EINVAL;
	}

	uint8_t current_addr, new_addr;
	if (parse_addr_arg(shell, argv[1], &current_addr) != 0) {
		return -EINVAL;
	}
	if (parse_addr_arg(shell, argv[2], &new_addr) != 0) {
		return -EINVAL;
	}

	shell_print(shell, "Setting Cubic PM address...");
	shell_warn(shell, "WARNING: Ensure ONLY the Cubic PM is connected and is at address %d.",
		   current_addr);

	int ret = app_modbus_set_cubic_pm_addr(current_addr, new_addr);
	if (ret) {
		shell_error(shell, "Failed to set address. Error: %d", ret);
	} else {
		shell_print(shell, "Command sent successfully. Target address is %d.", new_addr);
	}
	return ret;
}
static void print_sensecap_data(const struct shell *shell, const struct app_data_sensecap *data)
{
	// If we have measurements, display them (aggregated data)
	if (data->measurement_count > 0) {
		int current_idx = data->measurement_count - 1;
		shell_print(shell, "SenseCAP S500/S1000 Data (Avg of last aggregated period):");
		shell_print(shell, "  Air Temperature:     %.3f C",
			    (double)data->air_temperature_measurements[current_idx].avg);
		shell_print(shell, "  Air Humidity:        %.3f %%",
			    (double)data->air_humidity_measurements[current_idx].avg);
		shell_print(shell, "  Barometric Pressure: %.3f hPa",
			    (double)data->barometric_pressure_measurements[current_idx].avg);
		shell_print(shell, "  Wind Dir Avg:        %.3f deg",
			    (double)data->wind_direction_avg_measurements[current_idx].avg);
		shell_print(shell, "  Wind Speed Avg:      %.3f m/s",
			    (double)data->wind_speed_avg_measurements[current_idx].avg);

		if ((g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000)) {
			shell_print(shell, "  --- S1000 Specific ---");
			shell_print(shell, "  Light Intensity:     %.3f lux",
				    (double)data->light_intensity_measurements[current_idx].avg);
			shell_print(
				shell, "  Accum. Rainfall:     %.3f mm",
				(double)data->accumulated_rainfall_measurements[current_idx].avg);
			shell_print(shell, "  PM2.5:               %.3f ug/m3",
				    (double)data->pm2_5_measurements[current_idx].avg);
			shell_print(shell, "  PM10:                %.3f ug/m3",
				    (double)data->pm10_measurements[current_idx].avg);
			shell_print(shell, "  CO2:                 %.3f ppm",
				    (double)data->co2_measurements[current_idx].avg);
		}
	}
	// If we have samples but no measurements, display last sample
	else if (data->sample_count > 0) {
		int current_idx = data->sample_count - 1;
		shell_print(shell, "SenseCAP S500/S1000 Data (Last sample - NOT aggregated):");
		shell_print(shell, "  Air Temperature:     %.3f C",
			    (double)data->air_temperature_samples[current_idx]);
		shell_print(shell, "  Air Humidity:        %.3f %%",
			    (double)data->air_humidity_samples[current_idx]);
		shell_print(shell, "  Barometric Pressure: %.3f hPa",
			    (double)data->barometric_pressure_samples[current_idx]);
		shell_print(shell, "  Wind Dir Avg:        %.3f deg",
			    (double)data->wind_direction_avg_samples[current_idx]);
		shell_print(shell, "  Wind Speed Avg:      %.3f m/s",
			    (double)data->wind_speed_avg_samples[current_idx]);

		if ((g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000)) {
			shell_print(shell, "  --- S1000 Specific ---");
			shell_print(shell, "  Light Intensity:     %.3f lux",
				    (double)data->light_intensity_samples[current_idx]);
			shell_print(shell, "  Accum. Rainfall:     %.3f mm",
				    (double)data->accumulated_rainfall_samples[current_idx]);
			shell_print(shell, "  PM2.5:               %.3f ug/m3",
				    (double)data->pm2_5_samples[current_idx]);
			shell_print(shell, "  PM10:                %.3f ug/m3",
				    (double)data->pm10_samples[current_idx]);
			shell_print(shell, "  CO2:                 %.3f ppm",
				    (double)data->co2_samples[current_idx]);
		}
		shell_print(shell, "\nWARNING: Run 'aggreg' to process samples into measurements "
				   "for cloud sending!");
	}
	// No data at all
	else {
		shell_print(shell, "SenseCAP: No data available. Run 'sample' first.");
	}
}

static void print_cubic_pm_data(const struct shell *shell, const struct app_data_cubic_pm *data)
{
	// If we have measurements, display them (aggregated data)
	if (data->measurement_count > 0) {
		int current_idx = data->measurement_count - 1;
		shell_print(shell, "Cubic OPM PM Sensor Data (Avg of last aggregated period):");
		shell_print(shell, "  Version:             %.2f",
			    (double)data->version_measurements[current_idx].avg);
		shell_print(shell, "  PM1.0:               %.1f ug/m3",
			    (double)data->pm1_0_measurements[current_idx].avg);
		shell_print(shell, "  PM2.5:               %.1f ug/m3",
			    (double)data->pm2_5_measurements[current_idx].avg);
		shell_print(shell, "  PM10:                %.1f ug/m3",
			    (double)data->pm10_measurements[current_idx].avg);
		shell_print(shell, "  TSP:                 %.1f ug/m3",
			    (double)data->tsp_measurements[current_idx].avg);
		shell_print(shell, "  Gas Flow:            %.2f",
			    (double)data->gas_flow_measurements[current_idx].avg);
	}
	// If we have samples but no measurements, display last sample
	else if (data->sample_count > 0) {
		int current_idx = data->sample_count - 1;
		shell_print(shell, "Cubic OPM PM Sensor Data (Last sample - NOT aggregated):");
		shell_print(shell, "  Version:             %.2f",
			    (double)data->version_samples[current_idx]);
		shell_print(shell, "  PM1.0:               %.1f ug/m3",
			    (double)data->pm1_0_samples[current_idx]);
		shell_print(shell, "  PM2.5:               %.1f ug/m3",
			    (double)data->pm2_5_samples[current_idx]);
		shell_print(shell, "  PM10:                %.1f ug/m3",
			    (double)data->pm10_samples[current_idx]);
		shell_print(shell, "  TSP:                 %.1f ug/m3",
			    (double)data->tsp_samples[current_idx]);
		shell_print(shell, "  Gas Flow:            %.2f",
			    (double)data->gas_flow_samples[current_idx]);
		shell_print(shell, "\nWARNING: Run 'aggreg' to process samples into measurements "
				   "for cloud sending!");
	}
	// No data at all
	else {
		shell_print(shell, "Cubic PM: No data available. Run 'sample' first.");
	}
}

#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)
static void print_lambrecht_data(const struct shell *shell, const struct app_data_lambrecht *data)
{
	// If we have measurements, display them (aggregated data)
	if (data->measurement_count > 0) {
		int current_idx = data->measurement_count - 1;
		shell_print(shell, "Lambrecht Meteo Station Data (Avg of last aggregated period):");
		shell_print(shell, "  Wind Speed:          %.1f m/s",
			    (double)data->wind_speed_measurements[current_idx].avg);
		shell_print(shell, "  Wind Direction:      %.1f deg",
			    (double)data->wind_direction_measurements[current_idx].avg);
		shell_print(shell, "  Temperature:         %.1f C",
			    (double)data->temperature_measurements[current_idx].avg);
		shell_print(shell, "  Humidity:            %.1f %%",
			    (double)data->humidity_measurements[current_idx].avg);
		shell_print(shell, "  Dew Point:           %.1f C",
			    (double)data->dew_point_measurements[current_idx].avg);
		shell_print(shell, "  Pressure:            %.1f hPa",
			    (double)data->pressure_measurements[current_idx].avg);
		shell_print(shell, "  Rainfall Total:      %.1f mm",
			    (double)data->rainfall_total_measurements[current_idx].avg);
		shell_print(shell, "  Rainfall Intensity:  %.3f mm/min",
			    (double)data->rainfall_intensity_measurements[current_idx].avg);
		shell_print(shell, "  Illuminance:         %.3f lux",
			    (double)data->illuminance_measurements[current_idx].avg);
	}
	// If we have samples but no measurements, display last sample
	else if (data->sample_count > 0) {
		int current_idx = data->sample_count - 1;
		shell_print(shell, "Lambrecht Meteo Station Data (Last sample - NOT aggregated):");
		shell_print(shell, "  Wind Speed:          %.1f m/s",
			    (double)data->wind_speed_samples[current_idx]);
		shell_print(shell, "  Wind Direction:      %.1f deg",
			    (double)data->wind_direction_samples[current_idx]);
		shell_print(shell, "  Temperature:         %.1f C",
			    (double)data->temperature_samples[current_idx]);
		shell_print(shell, "  Humidity:            %.1f %%",
			    (double)data->humidity_samples[current_idx]);
		shell_print(shell, "  Dew Point:           %.1f C",
			    (double)data->dew_point_samples[current_idx]);
		shell_print(shell, "  Pressure:            %.1f hPa",
			    (double)data->pressure_samples[current_idx]);
		shell_print(shell, "  Rainfall Total:      %.1f mm",
			    (double)data->rainfall_total_samples[current_idx]);
		shell_print(shell, "  Rainfall Intensity:  %.3f mm/min",
			    (double)data->rainfall_intensity_samples[current_idx]);
		shell_print(shell, "  Illuminance:         %.3f lux",
			    (double)data->illuminance_samples[current_idx]);
		shell_print(shell, "\nWARNING: Run 'aggreg' to process samples into measurements "
				   "for cloud sending!");
	}
	// No data at all
	else {
		shell_print(shell, "Lambrecht: No data available. Run 'sample' first.");
	}
}
#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */

int cmd_modbus_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;
	bool sample_data = false;

	if (argc > 1) {
		if (strcmp(argv[1], "sample") == 0) {
			sample_data = true;
		} else {
			shell_error(shell, "unknown parameter: %s (try 'sample')", argv[1]);
			shell_help(shell);
			return -EINVAL;
		}
	}

	if (sample_data) {
		shell_print(shell, "Manually sampling Modbus devices...");
		app_modbus_enable();
		ret = app_modbus_sample();
		app_modbus_disable();

		if (ret != 0) {
			shell_error(shell, "Modbus sampling failed with error: %d", ret);
			return ret;
		}
	}

	app_data_lock();

	shell_print(shell, "\nModbus Data (based on last or fresh sample):");
	shell_print(shell, "--------------------------------------------");

	if ((g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S1000) ||
	    (g_app_config.meteo_type == APP_CONFIG_METEO_SENSECAP_S500)) {
		print_sensecap_data(shell, &g_app_data.sensecap);
	}
#if defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT)
	else if (g_app_config.meteo_type == APP_CONFIG_METEO_LAMBRECHT) {
		print_lambrecht_data(shell, &g_app_data.lambrecht);
	}
#endif /* defined(CONFIG_FEATURE_CHESTER_APP_LAMBRECHT) */
	else {
		shell_print(shell, "Meteo sensor: Disabled or unknown type.");
	}

	if (g_app_config.pm_type == APP_CONFIG_PM_CUBIC_6303) {
		print_cubic_pm_data(shell, &g_app_data.cubic_pm);
	} else {
		shell_print(shell, "Cubic PM: Disabled or unknown type.");
	}

	app_data_unlock();

	return 0;
}
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_M) */
