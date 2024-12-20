/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_send.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_LRW)

static int compose_lrw(struct ctr_buf *buf)
{
	int ret = 0;

	ctr_buf_reset(buf);

	app_data_lock();

	uint16_t header = 0;

	/* Flag BATT */
	if (IS_ENABLED(CONFIG_CTR_BATT)) {
		header |= BIT(0);
	}

	/* Flag ACCEL */
	if (IS_ENABLED(CONFIG_CTR_ACCEL)) {
		header |= BIT(1);
	}

	/* Flag THERM */
	if (IS_ENABLED(CONFIG_CTR_THERM)) {
		header |= BIT(2);
	}

	/* Flag IAQ */
	if (IS_ENABLED(FEATURE_HARDWARE_CHESTER_S1)) {
		header |= BIT(3);
	}

	/* Flag HYGRO */
	if (IS_ENABLED(FEATURE_HARDWARE_CHESTER_S2)) {
		header |= BIT(4);
	}

	/* Flag W1_THERM */
	if (IS_ENABLED(FEATURE_SUBSYSTEM_DS18B20)) {
		header |= BIT(5);
	}

	/* Flag RTD_THERM */
	if (IS_ENABLED(FEATURE_HARDWARE_CHESTER_RTD_A) ||
	    IS_ENABLED(FEATURE_HARDWARE_CHESTER_RTD_B)) {
		header |= BIT(6);
	}

	if (IS_ENABLED(FEATURE_SUBSYSTEM_BLE_TAG)) {
		header |= BIT(7);
		header |= BIT(8);
	}

	if (IS_ENABLED(FEATURE_SUBSYSTEM_SOIL_SENSOR)) {
		header |= BIT(7);
		header |= BIT(9);
	}

	ret |= ctr_buf_append_u8(buf, header);

	if (header & BIT(7)) {
		ret |= ctr_buf_append_u8(buf, 0xffff & (header >> 8));
	}

	/* Field BATT */
	if (header & BIT(0)) {
		if (isnan(g_app_data.system_voltage_rest)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = g_app_data.system_voltage_rest;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(g_app_data.system_voltage_load)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = g_app_data.system_voltage_load;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(g_app_data.system_current_load)) {
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
		} else {
			uint8_t val = g_app_data.system_current_load;
			ret |= ctr_buf_append_u8(buf, val);
		}
	}

	/* Field ACCEL */
	if (header & BIT(1)) {
		if (g_app_data.accel_orientation == INT_MAX) {
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
		} else {
			uint8_t val = g_app_data.accel_orientation;
			ret |= ctr_buf_append_u8(buf, val);
		}
	}

	/* Field THERM */
	if (header & BIT(2)) {
		if (isnan(g_app_data.therm_temperature)) {
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
		} else {
			int16_t val = g_app_data.therm_temperature * 100.f;
			ret |= ctr_buf_append_s16_le(buf, val);
		}
	}

#if defined(FEATURE_HARDWARE_CHESTER_S1)
	/* Field IAQ */
	if (header & BIT(3)) {
		struct app_data_iaq *iaqdata = &g_app_data.iaq;

		if (isnan(iaqdata->sensors.last_temperature)) {
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
		} else {
			int16_t val = iaqdata->sensors.last_temperature * 100.f;
			ret |= ctr_buf_append_s16_le(buf, val);
		}

		if (isnan(iaqdata->sensors.last_humidity)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = iaqdata->sensors.last_humidity * 100.f;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(iaqdata->sensors.last_altitude)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = iaqdata->sensors.last_altitude * 100.f;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(iaqdata->sensors.last_co2_conc)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = iaqdata->sensors.last_co2_conc;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(iaqdata->sensors.last_illuminance)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			uint16_t val = iaqdata->sensors.last_illuminance * 100.f;
			ret |= ctr_buf_append_u16_le(buf, val);
		}

		if (isnan(iaqdata->sensors.last_pressure)) {
			ret |= ctr_buf_append_u32_le(buf, BIT_MASK(16));
		} else {
			uint32_t val = iaqdata->sensors.last_pressure;
			ret |= ctr_buf_append_u32_le(buf, val);
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S1) */

#if defined(FEATURE_HARDWARE_CHESTER_S2)
	/* Field HYGRO */
	if (header & BIT(4)) {
		struct app_data_hygro *hygro = &g_app_data.hygro;

		if (isnan(hygro->last_sample_temperature)) {
			ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
		} else {
			ret |= ctr_buf_append_s16_le(buf, hygro->last_sample_temperature * 100.f);
		}

		if (isnan(hygro->last_sample_humidity)) {
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
		} else {
			ret |= ctr_buf_append_u8(buf, hygro->last_sample_humidity * 2.f);
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_S2) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	/* Field W1_THERM */
	if (header & BIT(5)) {
		float t[APP_DATA_W1_THERM_COUNT];

		int count = 0;

		for (size_t i = 0; i < APP_DATA_W1_THERM_COUNT; i++) {
			struct app_data_w1_therm_sensor *sensor = &g_app_data.w1_therm.sensor[i];

			if (!sensor->serial_number) {
				continue;
			}

			t[count++] = sensor->last_sample_temperature;
		}

		ret |= ctr_buf_append_u8(buf, count);

		for (size_t i = 0; i < count; i++) {
			if (isnan(t[i])) {
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
			} else {
				ret |= ctr_buf_append_s16_le(buf, t[i] * 100.f);
			}
		}
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_HARDWARE_CHESTER_RTD_A) || defined(FEATURE_HARDWARE_CHESTER_RTD_B)
	/* Field RTD_THERM */
	if (header & BIT(6)) {

		ret |= ctr_buf_append_u8(buf, APP_DATA_RTD_THERM_COUNT);

		for (int i = 0; i < APP_DATA_RTD_THERM_COUNT; i++) {
			struct app_data_rtd_therm_sensor *sensor = &g_app_data.rtd_therm.sensor[i];

			if (isnan(sensor->last_sample_temperature)) {
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
			} else {
				ret |= ctr_buf_append_s16_le(buf, sensor->last_sample_temperature *
									  100.f);
			}
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_RTD_A) || defined(FEATURE_HARDWARE_CHESTER_RTD_B) */

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	/* Field BLE_TAG */
	if (header & BIT(8)) {
		float temperature[CTR_BLE_TAG_COUNT];
		float humidity[CTR_BLE_TAG_COUNT];

		int count = 0;

		for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
			struct app_data_ble_tag_sensor *tag = &g_app_data.ble_tag.sensor[i];

			if (ctr_ble_tag_is_addr_empty(tag->addr)) {
				continue;
			}

			temperature[count] = tag->last_sample_temperature;
			humidity[count] = tag->last_sample_humidity;
			count++;
		}

		ret |= ctr_buf_append_u8(buf, count);

		for (size_t i = 0; i < count; i++) {
			if (isnan(temperature[i])) {
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
			} else {
				ret |= ctr_buf_append_s16_le(buf, temperature[i] * 100.f);
			}

			if (isnan(humidity[i])) {
				ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
			} else {
				ret |= ctr_buf_append_u8(buf, humidity[i] * 2.f);
			}
		}
	}

#endif /* defined(FEATURE_SUBSYSTEM_BLE_TAG) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	if (header & BIT(9)) {
		int count = 0;

		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {
			struct app_data_soil_sensor_sensor *sensor =
				&g_app_data.soil_sensor.sensor[i];

			if (sensor->serial_number == 0) {
				continue;
			}

			count++;
		}

		ret |= ctr_buf_append_u8(buf, count);

		for (int i = 0; i < APP_DATA_SOIL_SENSOR_COUNT; i++) {
			struct app_data_soil_sensor_sensor *sensor =
				&g_app_data.soil_sensor.sensor[i];

			if (sensor->serial_number == 0) {
				continue;
			}

			if (isnan(sensor->last_temperature)) {
				ret |= ctr_buf_append_s16_le(buf, BIT_MASK(15));
			} else {
				ret |= ctr_buf_append_s16_le(buf, sensor->last_temperature * 100.f);
			}

			if (isnan(sensor->last_moisture)) {
				ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
			} else {
				ret |= ctr_buf_append_u16(buf, sensor->last_moisture);
			}
		}
	}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

int app_send(void)
{
	__unused int ret;

#if defined(FEATURE_SUBSYSTEM_LRW)
	CTR_BUF_DEFINE_STATIC(lrw_buf, 51);
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

	switch (g_app_config.mode) {

#if defined(FEATURE_SUBSYSTEM_LRW)
	case APP_CONFIG_MODE_LRW:
		ret = compose_lrw(&lrw_buf);
		if (ret) {
			LOG_ERR("Call `compose_lrw` failed: %d", ret);
			return ret;
		}

		struct ctr_lrw_send_opts lrw_opts = CTR_LRW_SEND_OPTS_DEFAULTS;
		ret = ctr_lrw_send(&lrw_opts, ctr_buf_get_mem(&lrw_buf), ctr_buf_get_used(&lrw_buf),
				   NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
			return ret;
		}
		break;
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

	default:
		break;
	}

	return 0;
}

#if defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B)

static int compose_x4_line_alert(struct ctr_buf *buf, bool line_connected_event)
{
	int ret = 0;

	ctr_buf_reset(buf);

	ret |= ctr_buf_append_u8(buf, BIT(7));
	ret |= ctr_buf_append_u8(buf, line_connected_event ? 1 : 0);

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

int app_send_lrw_x4_line_alert(bool line_connected_event)
{
	int ret;

	CTR_BUF_DEFINE_STATIC(x4_lrw_buf, 2);

	ret = compose_x4_line_alert(&x4_lrw_buf, line_connected_event);
	if (ret) {
		LOG_ERR("Call `compose_x4_line_alert` failed: %d", ret);
		return ret;
	}

	struct ctr_lrw_send_opts lrw_opts = CTR_LRW_SEND_OPTS_DEFAULTS;

	ret = ctr_lrw_send(&lrw_opts, ctr_buf_get_mem(&x4_lrw_buf), ctr_buf_get_used(&x4_lrw_buf),
			   NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
		return ret;
	}

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X4_A) || defined(FEATURE_HARDWARE_CHESTER_X4_B) */
