/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "app_codec.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void ctr_cloud_event_handler(enum ctr_cloud_event event, union ctr_cloud_event_data *data,
				    void *param)
{
	if (event == CTR_CLOUD_EVENT_RECV) {
		if (data->recv.len < 8) {
			LOG_ERR("Missing encoder hash");
			return;
		}

		uint8_t *buf = (uint8_t *)data->recv.buf + 8;
		size_t len = data->recv.len - 8;

		LOG_HEXDUMP_INF(buf, len, "Received:");
	}
}

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	CODEC_CLOUD_OPTIONS_STATIC(copt);

	ret = ctr_cloud_init(&copt);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
		return;
	}

	ret = ctr_cloud_set_callback(ctr_cloud_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_set_callback` failed: %d", ret);
		return;
	}

	ctr_cloud_wait_initialized(K_FOREVER);

	for (;;) {
		LOG_INF("Alive");

		CTR_BUF_DEFINE_STATIC(buf, sizeof(uint32_t));

		static uint32_t counter;

		ctr_buf_reset(&buf);
		ret = ctr_buf_append_u32_be(&buf, counter++);
		if (ret) {
			LOG_ERR("Call to `ctr_buf_append_u32_be` failed: %d", ret);
			k_oops();
		}

		ret = ctr_cloud_send(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf));
		if (ret) {
			LOG_ERR("Call to `ctr_cloud_send` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(60));
	}
}
