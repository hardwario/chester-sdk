/* CHESTER includes */
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_chester_x0d.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/watchdog.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

const struct device *m_wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));

static uint64_t m_input_1_count;
static K_MUTEX_DEFINE(m_count_mut);

static void chester_x0d_event_handler(enum ctr_chester_x0d_slot slot,
                                      enum ctr_chester_x0d_channel channel,
                                      enum ctr_chester_x0d_event event, void *param)
{
	LOG_DBG("slot: %d channel: %d event: %d", (int)slot, (int)channel, (int)event);

	if (slot == CTR_CHESTER_X0D_SLOT_A && channel == CTR_CHESTER_X0D_CHANNEL_1) {
		if (event == CTR_CHESTER_X0D_EVENT_RISE) {
			LOG_INF("Channel 1 detected rising edge");
		}

		if (event == CTR_CHESTER_X0D_EVENT_FALL) {
			LOG_INF("Channel 1 detected falling edge");

			k_mutex_lock(&m_count_mut, K_FOREVER);
			m_input_1_count++;
			k_mutex_unlock(&m_count_mut);
		}
	}
}

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

		ctr_led_set(CTR_LED_CHANNEL_Y, true);
		k_sleep(K_SECONDS(2));
		ctr_led_set(CTR_LED_CHANNEL_Y, false);

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

		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(100));
		ctr_led_set(CTR_LED_CHANNEL_G, false);

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

	uint8_t buf[24] = { 0 };

	static uint64_t last;

	k_mutex_lock(&m_count_mut, K_FOREVER);
	uint64_t counter = m_input_1_count;
	k_mutex_unlock(&m_count_mut);

	uint64_t delta = counter - last;

	last = counter;

	LOG_INF("Sending data with counter value of %llu", counter);

	sys_put_le64(imsi, &buf[0]);
	sys_put_le64(counter, &buf[8]);
	sys_put_le64(delta, &buf[16]);

	struct ctr_lte_send_opts opts = CTR_LTE_SEND_OPTS_DEFAULTS;

	ret = ctr_lte_send(&opts, buf, sizeof(buf), NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_lte_send` failed: %d", ret);
	}
}

static int m_wdt_id;

static void init_wdt(void)
{
	int ret;

	if (!device_is_ready(m_wdt_dev)) {
		LOG_ERR("Call `device_is_ready` failed");
		k_oops();
	}

	struct wdt_timeout_cfg wdt_config = {
		.flags = WDT_FLAG_RESET_SOC,
		.window.min = 0U,
		.window.max = 120000,
	};

	ret = wdt_install_timeout(m_wdt_dev, &wdt_config);

	if (ret < 0) {
		LOG_ERR("Call `wdt_install_timeout` failed: %d", ret);
		k_oops();
	}

	m_wdt_id = ret;

	ret = wdt_setup(m_wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);

	if (ret < 0) {
		LOG_ERR("Call `wdt_setup` failed: %d", ret);
		k_oops();
	}
}

static void feed_wdt(void)
{
	if (!device_is_ready(m_wdt_dev)) {
		LOG_ERR("Call `device_is_ready` failed");
		k_oops();
	}

	wdt_feed(m_wdt_dev, m_wdt_id);
}

void main(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_G, true);
	k_sleep(K_SECONDS(2));
	ctr_led_set(CTR_LED_CHANNEL_G, false);

	init_wdt();

	ret = ctr_chester_x0d_init(CTR_CHESTER_X0D_SLOT_A, CTR_CHESTER_X0D_CHANNEL_1,
	                           chester_x0d_event_handler, NULL);

	if (ret < 0) {
		LOG_ERR("Call `ctr_chester_x0d_init` failed: %d", ret);
		k_oops();
	}

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

		feed_wdt();

		static int counter;

		if (counter++ % 5 == 0) {
			ctr_led_set(CTR_LED_CHANNEL_R, true);
			k_sleep(K_MSEC(100));
			ctr_led_set(CTR_LED_CHANNEL_R, false);

			send();
		}

		k_sleep(K_SECONDS(60));
	}
}
