#include "test_lte.h"
#include <ctr_net_lte.h>
#include <ctr_sys.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(test_lte, LOG_LEVEL_DBG);

void net_lte_callback(hio_net_lte_event_t *event, void *param)
{
	switch (event->source) {
	case HIO_NET_LTE_EVENT_ATTACH_DONE:
		LOG_INF("Event `HIO_NET_LTE_EVENT_ATTACH_DONE`");
		break;
	case HIO_NET_LTE_EVENT_ATTACH_ERROR:
		LOG_INF("Event `HIO_NET_LTE_EVENT_ATTACH_ERROR`");
		break;
	case HIO_NET_LTE_EVENT_DETACH_DONE:
		LOG_INF("Event `HIO_NET_LTE_EVENT_DETACH_DONE`");
		break;
	case HIO_NET_LTE_EVENT_DETACH_ERROR:
		LOG_INF("Event `HIO_NET_LTE_EVENT_DETACH_ERROR`");
		break;
	case HIO_NET_LTE_EVENT_SEND_DONE:
		LOG_INF("Event `HIO_NET_LTE_EVENT_SEND_DONE`");
		break;
	case HIO_NET_LTE_EVENT_SEND_ERROR:
		LOG_INF("Event `HIO_NET_LTE_EVENT_SEND_ERROR`");
		break;
	case HIO_NET_LTE_EVENT_RECV_DONE:
		LOG_INF("Event `HIO_NET_LTE_EVENT_RECV_DONE`");
		break;
	}
}

void test_lte(void)
{
	LOG_INF("Start");

	static const hio_net_lte_cfg_t cfg = HIO_NET_LTE_CFG_DEFAULTS;

	if (hio_net_lte_init(&cfg) < 0) {
		LOG_ERR("Call `hio_net_lte_init` failed");
	}

	if (hio_net_lte_set_callback(net_lte_callback, NULL) < 0) {
		LOG_ERR("Call `hio_net_lte_set_callback` failed");
	}

	if (hio_net_lte_attach() < 0) {
		LOG_ERR("Call `hio_net_lte_attach` failed");
	}

	uint8_t buf[] = { 1, 2, 3, 4 };

	for (;;) {
		LOG_DBG("Alive");

		hio_net_send_opts_t opts = HIO_NET_LTE_SEND_OPTS_DEFAULTS;

		if (hio_net_lte_send(&opts, buf, sizeof(buf)) < 0) {
			LOG_ERR("Call `hio_net_lte_send` failed");
		}

		hio_sys_task_sleep(HIO_SYS_MSEC(120 * 60000));
	}
}
