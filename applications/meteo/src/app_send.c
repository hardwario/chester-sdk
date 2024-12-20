/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_send.h"
#include "app_data.h"
#include "feature.h"

/* Zephyr includes */
#include <zephyr/logging/log.h>

/* Chester includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#if defined(FEATURE_SUBSYSTEM_LRW)
#include <chester/ctr_lrw.h>
#endif

/* Standard includes */
#include <math.h>

#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

#if defined(FEATURE_SUBSYSTEM_LRW)
static int compose_lrw(struct ctr_buf *buf)
{
	int ret = 0;

	ctr_buf_reset(buf);

	app_data_lock();

	uint8_t header = 0;

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

	/* Flag METEO */
	if (IS_ENABLED(FEATURE_HARDWARE_CHESTER_METEO_A) ||
	    IS_ENABLED(FEATURE_HARDWARE_CHESTER_METEO_B)) {
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

	/* Flag BLE_TAG */
	if (IS_ENABLED(FEATURE_SUBSYSTEM_BLE_TAG)) {
		header |= BIT(6);
	}

	/* Flag Barometer tag */
	if (IS_ENABLED(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)) {
		header |= BIT(7);
	}

	ctr_buf_append_u8(buf, header);

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

#if defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)
	if (header & BIT(3)) {
		struct app_data_meteo *meteo = &g_app_data.meteo;

		if (isnan(meteo->wind_speed.last_sample)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			ret |= ctr_buf_append_u16_le(buf, meteo->wind_speed.last_sample * 100.f);
		}

		if (isnan(meteo->wind_direction.last_sample)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			ret |= ctr_buf_append_u16_le(buf, meteo->wind_direction.last_sample);
		}

		if (isnan(meteo->rainfall.last_sample)) {
			ret |= ctr_buf_append_u16_le(buf, BIT_MASK(16));
		} else {
			ret |= ctr_buf_append_u16_le(buf, meteo->rainfall.last_sample * 100.f);
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_METEO_A) || defined(FEATURE_HARDWARE_CHESTER_METEO_B)   \
	*/

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

#if defined(FEATURE_SUBSYSTEM_BLE_TAG)
	/* Field BLE_TAG */
	if (header & BIT(6)) {
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

#if defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
	if (header & BIT(7)) {
		if (isnan(g_app_data.barometer.pressure.last_sample)) {
			ret |= ctr_buf_append_u32_le(buf, BIT64_MASK(32));
		} else {
			ret |= ctr_buf_append_u32_le(buf,
						     g_app_data.barometer.pressure.last_sample);
		}
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG) */

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

int app_send(void)
{
	int ret;

#if defined(FEATURE_SUBSYSTEM_LRW)
	CTR_BUF_DEFINE_STATIC(lrw_buf, 51);
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

	switch (g_app_config.mode) {
	case APP_CONFIG_MODE_NONE:
		LOG_WRN("Application mode is set to none. Not sending data.");
		break;
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
#if defined(FEATURE_SUBSYSTEM_CLOUD)
	case APP_CONFIG_MODE_LTE: {
		CTR_BUF_DEFINE_STATIC(buf, 8 * 1024);

		ctr_buf_reset(&buf);

		ZCBOR_STATE_E(zs, 8, ctr_buf_get_mem(&buf), ctr_buf_get_free(&buf), 1);

		ret = app_cbor_encode(zs);
		if (ret) {
			LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
			return ret;
		}

		size_t len = zs[0].payload_mut - ctr_buf_get_mem(&buf);

		ret = ctr_buf_seek(&buf, len);
		if (ret) {
			LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
			return ret;
		}

		ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
		if (ret) {
			LOG_ERR("Call `ctr_cloud_send` failed: %d", ret);
			return ret;
		}

		break;
	}
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */
	}

	return 0;
}