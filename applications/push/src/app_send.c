#if defined(CONFIG_SHIELD_CTR_LRW) && defined(CONFIG_SHIELD_CTR_LTE)
#error "Both shields ctr_lrw and ctr_lte cannot be enabled"
#endif /* defined(CONFIG_SHIELD_CTR_LRW) && defined(CONFIG_SHIELD_CTR_LTE) */

#include "app_send.h"
#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>
#include <zephyr/zephyr.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>
#include <zcbor_encode.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

#define REPORT_INTERVAL_MSEC (15 * 60 * 1000)

#if defined(CONFIG_SHIELD_CTR_LRW)

static int compose(struct ctr_buf *buf, struct app_data *data)
{
	int ret = 0;

	ctr_buf_reset(buf);

	uint32_t flags = 0;

	flags |= atomic_clear(&data->sources.button_x_click) ? BIT(0) : 0;
	flags |= atomic_clear(&data->sources.button_x_hold) ? BIT(1) : 0;
	flags |= atomic_clear(&data->sources.button_1_click) ? BIT(2) : 0;
	flags |= atomic_clear(&data->sources.button_1_hold) ? BIT(3) : 0;
	flags |= atomic_clear(&data->sources.button_2_click) ? BIT(4) : 0;
	flags |= atomic_clear(&data->sources.button_2_hold) ? BIT(5) : 0;
	flags |= atomic_clear(&data->sources.button_3_click) ? BIT(6) : 0;
	flags |= atomic_clear(&data->sources.button_3_hold) ? BIT(7) : 0;
	flags |= atomic_clear(&data->sources.button_4_click) ? BIT(8) : 0;
	flags |= atomic_clear(&data->sources.button_4_hold) ? BIT(9) : 0;

	flags |= data->states.line_present ? BIT(29) : 0;
	flags |= data->errors.line_present ? BIT(30) : 0;

	flags |= atomic_clear(&data->sources.device_boot) ? BIT(31) : 0;

	ret |= ctr_buf_append_u32(buf, flags);

	if (data->errors.orientation) {
		ret |= ctr_buf_append_u8(buf, 0);
	} else {
		ret |= ctr_buf_append_u8(buf, data->states.orientation);
	}

	if (data->errors.int_temperature) {
		ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
	} else {
		int16_t val = data->states.int_temperature * 100.f;
		ret |= ctr_buf_append_s16(buf, val);
	}

	if (data->errors.ext_temperature) {
		ret |= ctr_buf_append_s16(buf, BIT_MASK(15));
	} else {
		int16_t val = data->states.ext_temperature * 100.f;
		ret |= ctr_buf_append_s16(buf, val);
	}

	if (data->errors.ext_humidity) {
		ret |= ctr_buf_append_u8(buf, BIT_MASK(8));
	} else {
		uint8_t val = data->states.ext_humidity * 2.f;
		ret |= ctr_buf_append_u8(buf, val);
	}

	if (data->errors.line_voltage) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.line_voltage * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.bckp_voltage) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.bckp_voltage * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_voltage_rest) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.batt_voltage_rest * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_voltage_load) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.batt_voltage_load * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	if (data->errors.batt_current_load) {
		ret |= ctr_buf_append_u16(buf, BIT_MASK(16));
	} else {
		uint16_t val = data->states.batt_current_load * 1000.f;
		ret |= ctr_buf_append_u16(buf, val);
	}

	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.device_tilt));

	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_x_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_x_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_1_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_1_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_2_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_2_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_3_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_3_hold));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_4_click));
	ret |= ctr_buf_append_u16(buf, atomic_get(&data->events.button_4_hold));

	if (ret) {
		return -ENOSPC;
	}

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_LRW) */

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

void send_timer(struct k_timer *timer_id)
{
	LOG_INF("Send timer expired");

	atomic_set(&g_app_loop_send, true);
	k_sem_give(&g_app_loop_sem);
}

K_TIMER_DEFINE(g_app_send_timer, send_timer, NULL);

int app_send(void)
{
#if defined(CONFIG_SHIELD_CTR_LRW) || defined(CONFIG_SHIELD_CTR_LTE)
	int ret;
#endif /* defined(CONFIG_SHIELD_CTR_LRW) || defined(CONFIG_SHIELD_CTR_LTE) */

	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.report_interval * 1000 / 5);
	int64_t duration = g_app_config.report_interval * 1000 + jitter;

	LOG_INF("Scheduling next report in %lld second(s)", duration / 1000);

	k_timer_start(&g_app_send_timer, K_MSEC(duration), K_FOREVER);

#if defined(CONFIG_SHIELD_CTR_LRW)
	CTR_BUF_DEFINE_STATIC(buf, 51);

	ret = compose(&buf, &g_app_data);
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
	CTR_BUF_DEFINE_STATIC(buf, 512);

	ret = compose(&buf);
	if (ret) {
		LOG_ERR("Call `compose` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_eval(NULL);
	if (ret == -ENOMSG) {
		LOG_WRN("LTE queue is full");
		return 0;
	} else if (ret) {
		LOG_ERR("Call `ctr_lte_eval` failed: %d", ret);
		return ret;
	}

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;
	opts.port = 5000;
	ret = ctr_lte_send(&opts, ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), NULL);
	if (ret == -ENOMSG) {
		LOG_WRN("LTE queue is full");
		return 0;
	} else if (ret) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

	return 0;
}
