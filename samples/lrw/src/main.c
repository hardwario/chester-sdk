#include <hio_net_lrw.h>

#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

static void lrw_event_cb(enum hio_net_lrw_event event, union hio_net_lrw_event_data *data,
                         void *param)
{
	int ret;

	LOG_DBG("hio_net_lrw_event: %d, hio_net_lrw_event_data: %p, param: %p", event, data, param);

	switch (event) {
	case HIO_NET_LRW_EVENT_FAILURE:
		ret = hio_net_lrw_start(NULL);
		if (ret < 0) {
			LOG_ERR("Call `hio_net_lrw_start` failed: %d", ret);
			k_oops();
		}
		break;
	case HIO_NET_LRW_EVENT_START_OK:
		ret = hio_net_lrw_join(NULL);
		if (ret < 0) {
			LOG_ERR("Call `hio_net_lrw_join` failed: %d", ret);
			k_oops();
		}
		break;
	default:
		LOG_DBG("unknown event: %d", event);
		break;
	}
}

void main(void)
{
	int ret;

	LOG_INF("LRW Example");

	ret = hio_net_lrw_init(lrw_event_cb, NULL);
	if (ret < 0) {
		LOG_ERR("Call `hio_net_lrw_init` failed: %d", ret);
		k_oops();
	}

	ret = hio_net_lrw_start(NULL);
	if (ret < 0) {
		LOG_ERR("Call `hio_net_lrw_start` failed: %d", ret);
		k_oops();
	}

	struct hio_net_lrw_send_opts lrw_send_opts = HIO_NET_LRW_SEND_OPTS_DEFAULTS;
	int counter = 0;

	while (1) {
		LOG_DBG("Alive");

		ret = hio_net_lrw_send(&lrw_send_opts, &counter, sizeof(counter), NULL);
		if (ret < 0) {
			LOG_WRN("Call `hio_net_lrw_send` failed: %d", ret);
		}

		counter++;

		k_sleep(K_SECONDS(30));
	}
}
