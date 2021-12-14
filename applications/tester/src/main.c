/* CHESTER includes */
#include <ctr_accel.h>
#include <ctr_batt.h>
#include <ctr_led.h>
#include <ctr_lte.h>
#include <ctr_hygro.h>
#include <ctr_therm.h>

/* Zephyr includes */
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static bool m_attached;

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
		m_attached = true;
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

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

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

	ret = ctr_therm_init();

	if (ret < 0) {
		LOG_ERR("Call `ctr_therm_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		int step = 0;

		LOG_WRN("Test %d: Attach", ++step);

		LOG_INF("Attached: %s", m_attached ? "YES" : "NO");

		LOG_WRN("Test %d: LEDs", ++step);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_R, false);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_G, true);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_G, false);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_Y, true);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_Y, false);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_EXT, m_attached ? false : true);
		k_sleep(K_MSEC(500));
		ctr_led_set(CTR_LED_CHANNEL_EXT, m_attached ? true : false);

		float temperature;

		LOG_WRN("Test %d: Thermometer", ++step);

		ret = ctr_therm_read(&temperature);

		if (ret < 0) {
			LOG_ERR("Call `ctr_therm_read` failed: %d", ret);

		} else {
			LOG_INF("Temperature: %.1f C", temperature);
		}

		LOG_WRN("Test %d: Hygrometer", ++step);

		float humidity;

		ret = ctr_hygro_read(&temperature, &humidity);

		if (ret < 0) {
			LOG_ERR("Call `ctr_hygro_read` failed: %d", ret);

		} else {
			LOG_INF("Temperature: %.1f C", temperature);
			LOG_INF("Humidity: %.1f %%", humidity);
		}

		float accel_x;
		float accel_y;
		float accel_z;

		ret = ctr_accel_read(&accel_x, &accel_y, &accel_z, NULL);

		if (ret < 0) {
			LOG_ERR("Call `ctr_accel_read` failed: %d", ret);

		} else {
			LOG_INF("Axis X: %.3f", accel_x);
			LOG_INF("Axis Y: %.3f", accel_y);
			LOG_INF("Axis Z: %.3f", accel_z);
		}

		struct ctr_batt_result batt_result;

		ret = ctr_batt_measure(&batt_result);

		if (ret < 0) {
			LOG_ERR("Call `ctr_batt_measure` failed: %d", ret);

		} else {
			LOG_INF("Voltage (rest) mV: %u mV", batt_result.voltage_rest_mv);
			LOG_INF("Voltage (load) mV: %u mV", batt_result.voltage_load_mv);
		}

		k_sleep(K_MSEC(500));
	}
}
