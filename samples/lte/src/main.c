/* CHESTER includes */
#include <ctr_lte.h>

/* Zephyr includes */
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void start(void)
{
	int ret;

	ret = ctr_lte_start(NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}
}

static void attach(void)
{
	int ret;

	ret = ctr_lte_attach(NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		k_oops();
	}
}

static void lte_event_handler(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param)
{
	ARG_UNUSED(param);

	switch (event) {
	case CTR_LTE_EVENT_FAILURE:
		LOG_ERR("Event `CTR_LTE_EVENT_FAILURE`");
		start();
		break;

	case CTR_LTE_EVENT_START_OK:
		LOG_INF("Event `CTR_LTE_EVENT_START_OK`");
		attach();
		break;

	case CTR_LTE_EVENT_START_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_START_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_ATTACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_ATTACH_OK`");
		break;

	case CTR_LTE_EVENT_ATTACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_ATTACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_DETACH_OK:
		LOG_INF("Event `CTR_LTE_EVENT_DETACH_OK`");
		break;

	case CTR_LTE_EVENT_DETACH_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_DETACH_ERR`");
		start();
		break;

	case CTR_LTE_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LTE_EVENT_SEND_OK`");
		break;

	case CTR_LTE_EVENT_SEND_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_SEND_ERR`");
		start();
		break;

	default:
		LOG_WRN("Unknown event: %d", event);
		return;
	}
}

static void send(void)
{
	int ret;

	uint64_t imsi;

	ret = ctr_lte_get_imsi(&imsi);

	if (ret < 0) {
		return;
	}

	uint8_t buf[12] = { 0 };

	static uint32_t counter;

	sys_put_le64(imsi, &buf[0]);
	sys_put_le32(counter++, &buf[8]);

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;

	ret = ctr_lte_send(&opts, buf, sizeof(buf), NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
	}
}

void main(void)
{
	int ret;

	ret = ctr_lte_set_event_cb(lte_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		k_oops();
	}

	ret = ctr_lte_start(NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		send();

		k_sleep(K_SECONDS(60));
	}
}
