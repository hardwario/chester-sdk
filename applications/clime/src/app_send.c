#if defined(CONFIG_SHIELD_CTR_LRW) && defined(CONFIG_SHIELD_CTR_LTE)
#error "Both shields ctr_lrw and ctr_lte cannot be enabled"
#endif /* defined(CONFIG_SHIELD_CTR_LRW) && defined(CONFIG_SHIELD_CTR_LTE) */

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

static int compose(struct ctr_buf *buf)
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
			ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
		} else {
			uint16_t val = g_app_data.system_voltage_rest * 1000.f;
			ret |= ctr_buf_append_u16(buf, val);
		}

		if (isnan(g_app_data.system_voltage_load)) {
			ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
		} else {
			uint16_t val = g_app_data.system_voltage_load * 1000.f;
			ret |= ctr_buf_append_u16(buf, val);
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
			ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
		} else {
			int16_t val = g_app_data.therm_temperature * 100.f;
			ret |= ctr_buf_append_s16(buf, val);
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

		float t = NAN;
		float h = NAN;

		if (hygro->sample_count) {
			t = hygro->samples_temperature[hygro->sample_count - 1];
			h = hygro->samples_humidity[hygro->sample_count - 1];
		}

		if (isnan(t)) {
			ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
		} else {
			ret |= ctr_buf_append_s16(buf, t * 100.f);
		}

		if (isnan(h)) {
			ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
		} else {
			ret |= ctr_buf_append_u8(buf, h * 2.f);
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

			float *samples = sensor->samples_temperature;

			if (sensor->sample_count) {
				t[count++] = samples[sensor->sample_count - 1];
			} else {
				t[count++] = NAN;
			}

			sensor->sample_count = 0;
		}

		ret |= ctr_buf_append_u8(buf, count);

		for (size_t i = 0; i < count; i++) {
			if (isnan(t[i])) {
				ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
			} else {
				ret |= ctr_buf_append_s16(buf, t[i] * 100.f);
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
			float t = NAN;

			if (sensor->sample_count) {
				t = sensor->samples_temperature[sensor->sample_count - 1];
			}

			if (isnan(t)) {
				ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
			} else {
				ret |= ctr_buf_append_s16(buf, t * 100.f);
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

#endif

#if defined(CONFIG_SHIELD_CTR_LTE)

static int compose(struct ctr_buf *buf)
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

	ret = ctr_buf_append_u32(buf, serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u32` failed: %d", ret);
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

	ret = ctr_buf_append_u16(buf, len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u16` failed: %d", ret);
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
#if defined(CONFIG_SHIELD_CTR_LRW) || defined(CONFIG_SHIELD_CTR_LTE)
	int ret;
#endif /* defined(CONFIG_SHIELD_CTR_LRW) || defined(CONFIG_SHIELD_CTR_LTE) */

#if defined(CONFIG_SHIELD_CTR_LRW)
	CTR_BUF_DEFINE_STATIC(buf, 51);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	struct ctr_lrw_send_opts opts = CTR_LRW_SEND_OPTS_DEFAULTS;
	ret = ctr_lrw_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_send` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

#if defined(CONFIG_SHIELD_CTR_LTE)
	CTR_BUF_DEFINE_STATIC(buf, 1024);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_eval(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 5000;
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

	return 0;
}
