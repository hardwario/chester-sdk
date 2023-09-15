/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_send.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
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

#if defined(CONFIG_SHIELD_CTR_LRW)

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

	/* Flag IAQ */
	if (IS_ENABLED(CONFIG_SHIELD_CTR_S1)) {
		header |= BIT(3);
	}

	/* Flag HYGRO */
	if (IS_ENABLED(CONFIG_SHIELD_CTR_S2)) {
		header |= BIT(4);
	}

	/* Flag W1_THERM */
	if (IS_ENABLED(CONFIG_SHIELD_CTR_DS18B20)) {
		header |= BIT(5);
	}

	/* Flag RTD_THERM */
	if (IS_ENABLED(CONFIG_SHIELD_CTR_RTD_A) || IS_ENABLED(CONFIG_SHIELD_CTR_RTD_B)) {
		header |= BIT(6);
	}

	ret |= ctr_buf_append_u8(buf, header);

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

#if defined(CONFIG_SHIELD_CTR_S1)
	/* Field IAQ */
	if (header & BIT(3)) {
		/* TODO Implement */
	}
#endif /* defined(CONFIG_SHIELD_CTR_S1) */

#if defined(CONFIG_SHIELD_CTR_S2)
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
#endif /* defined(CONFIG_SHIELD_CTR_S2) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
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
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

#if defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B)
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
#endif /* defined(CONFIG_SHIELD_CTR_RTD_A) || defined(CONFIG_SHIELD_CTR_RTD_B) */

	app_data_unlock();

	if (ret) {
		return -EFAULT;
	}

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

#if defined(CONFIG_SHIELD_CTR_LTE)

static int compose_lte(struct ctr_buf *buf)
{
	int ret;

	ctr_buf_reset(buf);
	ret = ctr_buf_seek(buf, 8);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_info_get_serial_number_uint32(&serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_info_get_serial_number_uint32` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u32_le(buf, serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u32_le` failed: %d", ret);
		return ret;
	}

	zcbor_state_t zs[8];
	zcbor_new_state(zs, ARRAY_SIZE(zs), ctr_buf_get_mem(buf) + 12, ctr_buf_get_free(buf), 0);

	ret = app_cbor_encode(zs);
	if (ret) {
		LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
		return ret;
	}

	size_t len = zs[0].payload_mut - ctr_buf_get_mem(buf);

	struct tc_sha256_state_struct s;
	ret = tc_sha256_init(&s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_init` failed: %d", ret);
		return ret;
	}

	ret = tc_sha256_update(&s, ctr_buf_get_mem(buf) + 8, len - 8);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_update` failed: %d", ret);
		return ret;
	}

	uint8_t digest[32];
	ret = tc_sha256_final(digest, &s);
	if (ret != TC_CRYPTO_SUCCESS) {
		LOG_ERR("Call `tc_sha256_final` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, 0);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u16_le(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u16_le` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_mem(buf, digest, 6);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_seek(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

int app_send(void)
{
	int ret;

#if defined(CONFIG_SHIELD_CTR_LRW)
	CTR_BUF_DEFINE_STATIC(lrw_buf, 51);
#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

#if defined(CONFIG_SHIELD_CTR_LTE)
	CTR_BUF_DEFINE_STATIC(lte_buf, 1024);
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

	switch (g_app_config.mode) {

#if defined(CONFIG_SHIELD_CTR_LRW)
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
#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

#if defined(CONFIG_SHIELD_CTR_LTE)
	case APP_CONFIG_MODE_LTE:
		ret = compose_lte(&lte_buf);
		if (ret) {
			LOG_ERR("Call `compose_lte` failed: %d", ret);
			return ret;
		}

		ret = ctr_lte_eval(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
			return ret;
		}

		struct ctr_lte_send_opts lte_opts = CTR_LTE_SEND_OPTS_DEFAULTS;
		lte_opts.port = 5000;
		ret = ctr_lte_send(&lte_opts, ctr_buf_get_mem(&lte_buf), ctr_buf_get_used(&lte_buf),
				   NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
			return ret;
		}
		break;
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

	default:
		break;
	}

	return 0;
}

#if defined(CONFIG_SHIELD_CTR_X4_A) || defined(CONFIG_SHIELD_CTR_X4_B)

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

#endif /* defined(CONFIG_SHIELD_CTR_X4_A) || defined(CONFIG_SHIELD_CTR_X4_B) */
