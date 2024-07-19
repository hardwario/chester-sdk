#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <chester/ctr_lrw_v2.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static K_MUTEX_DEFINE(m_status_lock);

static void lrw_v2_event_handler(enum ctr_lrw_v2_event event, const char *line, void *param)
{
	int ret;

	switch (event) {
	case CTR_LRW_V2_EVENT_FAILURE:
		ret = ctr_lrw_v2_start();
		if (ret) {
			LOG_ERR("Call `ctr_lrw_v2_start` failed: %d", ret);
			k_oops();
		}
		break;
	case CTR_LRW_V2_EVENT_START_OK:
		break;
	case CTR_LRW_V2_EVENT_START_ERR:
		break;
	case CTR_LRW_V2_EVENT_JOIN_OK:
		LOG_DBG("Joined network");
		k_mutex_unlock(&m_status_lock);
		break;
	case CTR_LRW_V2_EVENT_JOIN_ERR:
		break;
	case CTR_LRW_V2_EVENT_SEND_OK:
		break;
	case CTR_LRW_V2_EVENT_SEND_ERR:
		break;
	case CTR_LRW_V2_EVENT_RECV:
		LOG_DBG("Got downlink message: \"%s\"", line);
		break;
	default:
		LOG_DBG("Unknown event: %d", event);
		break;
	}
}

int main(void)
{
	int ret;

	k_mutex_lock(&m_status_lock, K_FOREVER);

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_lrw_v2_init(lrw_v2_event_handler);
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_v2_start();
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_start` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lrw_v2_join();
	if (ret) {
		LOG_ERR("Call `ctr_lrw_v2_join` failed: %d", ret);
		k_oops();
	}

	k_mutex_lock(&m_status_lock, K_FOREVER);

	struct ctr_lrw_send_opts lrw_send_opts = CTR_LRW_SEND_OPTS_DEFAULTS;
	int counter = 0;

	while (1) {
		LOG_DBG("Alive");

		ret = ctr_lrw_v2_send(&lrw_send_opts, &counter, sizeof(counter));
		if (ret) {
			LOG_ERR("Call `ctr_lrw_v2_send` failed: %d", ret);
		}

		k_sleep(K_SECONDS(30));

		counter++;
	}

	return 0;
}
