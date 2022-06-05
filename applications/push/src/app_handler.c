#include "app_handler.h"
#include "app_data.h"
#include "app_init.h"
#include "app_loop.h"

/* CHESTER includes */
#if defined(CONFIG_SHIELD_CTR_LRW)
#include <ctr_lrw.h>
#endif

#if defined(CONFIG_SHIELD_CTR_LTE)
#include <ctr_lte.h>
#endif

#include <drivers/ctr_z.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(app_handler, LOG_LEVEL_DBG);

#if defined(CONFIG_SHIELD_CTR_LRW)

void app_handler_lrw(enum ctr_lrw_event event, union ctr_lrw_event_data *data, void *param)
{
	int ret;

	switch (event) {
	case CTR_LRW_EVENT_FAILURE:
		LOG_INF("Event `CTR_LRW_EVENT_FAILURE`");
		ret = ctr_lrw_start(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
		}
		break;
	case CTR_LRW_EVENT_START_OK:
		LOG_INF("Event `CTR_LRW_EVENT_START_OK`");
		ret = ctr_lrw_join(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_join` failed: %d", ret);
		}
		break;
	case CTR_LRW_EVENT_START_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_START_ERR`");
		break;
	case CTR_LRW_EVENT_JOIN_OK:
		LOG_INF("Event `CTR_LRW_EVENT_JOIN_OK`");
		break;
	case CTR_LRW_EVENT_JOIN_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_JOIN_ERR`");
		break;
	case CTR_LRW_EVENT_SEND_OK:
		LOG_INF("Event `CTR_LRW_EVENT_SEND_OK`");
		break;
	case CTR_LRW_EVENT_SEND_ERR:
		LOG_INF("Event `CTR_LRW_EVENT_SEND_ERR`");
		break;
	default:
		LOG_WRN("Unknown event: %d", event);
	}
}

#endif

#if defined(CONFIG_SHIELD_CTR_LTE)

static void start(void)
{
	int ret;

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		k_oops();
	}
}

static void attach(void)
{
	int ret;

	ret = ctr_lte_attach(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_attach` failed: %d", ret);
		k_oops();
	}
}

void app_handler_lte(enum ctr_lte_event event, union ctr_lte_event_data *data, void *param)
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
		k_sem_give(&g_app_init_sem);
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

	case CTR_LTE_EVENT_EVAL_OK:
		LOG_INF("Event `CTR_LTE_EVENT_EVAL_OK`");

		struct ctr_lte_eval *eval = &data->eval_ok.eval;

		LOG_DBG("EEST: %d", eval->eest);
		LOG_DBG("ECL: %d", eval->ecl);
		LOG_DBG("RSRP: %d", eval->rsrp);
		LOG_DBG("RSRQ: %d", eval->rsrq);
		LOG_DBG("SNR: %d", eval->snr);
		LOG_DBG("PLMN: %d", eval->plmn);
		LOG_DBG("CID: %d", eval->cid);
		LOG_DBG("BAND: %d", eval->band);
		LOG_DBG("EARFCN: %d", eval->earfcn);

		k_mutex_lock(&g_app_data_lte_eval_mut, K_FOREVER);
		memcpy(&g_app_data_lte_eval, &data->eval_ok.eval, sizeof(g_app_data_lte_eval));
		g_app_data_lte_eval_valid = true;
		k_mutex_unlock(&g_app_data_lte_eval_mut);

		break;

	case CTR_LTE_EVENT_EVAL_ERR:
		LOG_ERR("Event `CTR_LTE_EVENT_EVAL_ERR`");

		k_mutex_lock(&g_app_data_lte_eval_mut, K_FOREVER);
		g_app_data_lte_eval_valid = false;
		k_mutex_unlock(&g_app_data_lte_eval_mut);

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

#endif

static int handle_button(enum ctr_z_event event, enum ctr_z_event match,
                         enum ctr_z_led_channel led_channel, enum ctr_z_led_command led_command,
                         enum ctr_z_buzzer_command buzzer_command, atomic_t *counter,
                         atomic_t *source)
{
	int ret;

	if (event != match) {
		return 0;
	}

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	struct ctr_z_led_param led_param = {
		.brightness = CTR_Z_LED_BRIGHTNESS_HIGH,
		.command = led_command,
		.pattern = CTR_Z_LED_PATTERN_NONE,
	};

	ret = ctr_z_set_led(dev, led_channel, &led_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_led` failed: %d", ret);
		return ret;
	}

	struct ctr_z_buzzer_param buzzer_param = {
		.command = buzzer_command,
		.pattern = CTR_Z_BUZZER_PATTERN_NONE,
	};

	ret = ctr_z_set_buzzer(dev, &buzzer_param);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_buzzer` failed: %d", ret);
		return ret;
	}

	atomic_inc(counter);
	atomic_set(source, true);

	atomic_set(&g_app_loop_send, true);
	k_sem_give(&g_app_loop_sem);

	return 1;
}

void app_handler_ctr_z(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	int ret;

	LOG_INF("Event: %d", event);

#define HANDLE_CLICK(button, counter, source)                                                      \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button##_CLICK,                    \
		                    CTR_Z_LED_CHANNEL_##button##_G, CTR_Z_LED_COMMAND_1X_1_2,      \
		                    CTR_Z_BUZZER_COMMAND_1X_1_2, counter, source);                 \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_CLICK(0, &g_app_data.events.button_x_click, &g_app_data.sources.button_x_click);
	HANDLE_CLICK(1, &g_app_data.events.button_1_click, &g_app_data.sources.button_1_click);
	HANDLE_CLICK(2, &g_app_data.events.button_2_click, &g_app_data.sources.button_2_click);
	HANDLE_CLICK(3, &g_app_data.events.button_3_click, &g_app_data.sources.button_3_click);
	HANDLE_CLICK(4, &g_app_data.events.button_4_click, &g_app_data.sources.button_4_click);

#undef HANDLE_CLICK

#define HANDLE_HOLD(button, counter, source)                                                       \
	do {                                                                                       \
		ret = handle_button(event, CTR_Z_EVENT_BUTTON_##button##_HOLD,                     \
		                    CTR_Z_LED_CHANNEL_##button##_R, CTR_Z_LED_COMMAND_2X_1_2,      \
		                    CTR_Z_BUZZER_COMMAND_2X_1_2, counter, source);                 \
		if (ret < 0) {                                                                     \
			LOG_ERR("Call `handle_button` failed: %d", ret);                           \
		} else if (ret) {                                                                  \
			goto apply;                                                                \
		}                                                                                  \
	} while (0)

	HANDLE_HOLD(0, &g_app_data.events.button_x_hold, &g_app_data.sources.button_x_hold);
	HANDLE_HOLD(1, &g_app_data.events.button_1_hold, &g_app_data.sources.button_1_hold);
	HANDLE_HOLD(2, &g_app_data.events.button_2_hold, &g_app_data.sources.button_2_hold);
	HANDLE_HOLD(3, &g_app_data.events.button_3_hold, &g_app_data.sources.button_3_hold);
	HANDLE_HOLD(4, &g_app_data.events.button_4_hold, &g_app_data.sources.button_4_hold);

#undef HANDLE_HOLD

	switch (event) {
	case CTR_Z_EVENT_DEVICE_RESET:
		LOG_INF("Event `CTR_Z_EVENT_DEVICE_RESET`");
		goto apply;

	case CTR_Z_EVENT_DC_CONNECTED:
		LOG_INF("Event `CTR_Z_EVENT_DC_CONNECTED`");
		atomic_set(&g_app_loop_send, true);
		k_sem_give(&g_app_loop_sem);
		break;

	case CTR_Z_EVENT_DC_DISCONNECTED:
		LOG_INF("Event `CTR_Z_EVENT_DC_DISCONNECTED`");
		atomic_set(&g_app_loop_send, true);
		k_sem_give(&g_app_loop_sem);
		break;
	default:
		break;
	}

apply:
	ret = ctr_z_apply(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_apply` failed: %d", ret);
	}
}
