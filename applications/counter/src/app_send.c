#include "app_send.h"
#include "app_cbor.h"
#include "app_config.h"
#include "app_data.h"
#include "app_loop.h"

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lte.h>

/* Zephyr includes */
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>
#include <zephyr/zephyr.h>
#include <tinycbor/cbor.h>
#include <tinycbor/cbor_buf_writer.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>

LOG_MODULE_REGISTER(app_send, LOG_LEVEL_DBG);

static void send_timer(struct k_timer *timer_id)
{
	LOG_INF("Send timer expired");

	atomic_set(&g_app_loop_send, true);
	k_sem_give(&g_app_loop_sem);
}

K_TIMER_DEFINE(g_app_send_timer, send_timer, NULL);

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

	struct cbor_buf_writer writer;
	cbor_buf_writer_init(&writer, ctr_buf_get_mem(buf) + 12, ctr_buf_get_free(buf));

	CborEncoder enc;
	cbor_encoder_init(&enc, &writer.enc, 0);

	ret = app_cbor_encode(&enc);
	if (ret) {
		LOG_ERR("Call `app_cbor_encode` failed: %d", ret);
		return ret;
	}

	size_t len = 12 + enc.writer->bytes_written;

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

int app_send(void)
{
	int ret;

	int64_t jitter = (int32_t)sys_rand32_get() % (g_app_config.report_interval * 1000 / 5);
	int64_t duration = g_app_config.report_interval * 1000 + jitter;

	LOG_INF("Scheduling next report in %lld second(s)", duration / 1000);

	k_timer_start(&g_app_send_timer, K_MSEC(duration), K_FOREVER);

	CTR_BUF_DEFINE_STATIC(buf, 512);

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

	k_mutex_lock(&g_app_data_lock, K_FOREVER);

#if defined(CONFIG_SHIELD_CTR_X0_A)
	g_app_data.counter_ch1_delta = 0;
	g_app_data.counter_ch2_delta = 0;
	g_app_data.counter_ch3_delta = 0;
	g_app_data.counter_ch4_delta = 0;
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_X0_B)
	g_app_data.counter_ch5_delta = 0;
	g_app_data.counter_ch6_delta = 0;
	g_app_data.counter_ch7_delta = 0;
	g_app_data.counter_ch8_delta = 0;
#endif /* defined(CONFIG_SHIELD_CTR_X0_B) */

	k_mutex_unlock(&g_app_data_lock);

	return 0;
}
