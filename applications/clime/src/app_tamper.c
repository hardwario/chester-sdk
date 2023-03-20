#include "app_data.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_edge.h>
#include <chester/ctr_rtc.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

LOG_MODULE_REGISTER(app_tamper, LOG_LEVEL_DBG);

static void edge_callback(struct ctr_edge *edge, enum ctr_edge_event event, void *user_data)
{
	int ret;

	LOG_INF("Event: %d", event);

	struct app_data_tamper *tamper = &g_app_data.tamper;

	app_data_lock();

	tamper->activated = event == CTR_EDGE_EVENT_ACTIVE;

	if (tamper->event_count < APP_DATA_MAX_TAMPER_EVENTS) {
		struct app_data_tamper_event *te = &tamper->events[tamper->event_count];

		ret = ctr_rtc_get_ts(&te->timestamp);
		if (ret) {
			LOG_ERR("Call `ctr_rtc_get_ts` failed: %d", ret);
			app_data_unlock();
			return;
		}

		te->activated = event == CTR_EDGE_EVENT_ACTIVE;
		tamper->event_count++;

		LOG_INF("Event count: %d", tamper->event_count);
	} else {
		LOG_WRN("Event buffer full");
		app_data_unlock();
		return;
	}

	app_work_send_with_rate_limit();

	app_data_unlock();
}

int app_tamper_init(void)
{
	int ret;

	LOG_INF("System initialization");

	static const struct gpio_dt_spec tamper_spec =
		GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), tamper_gpios);

	if (!device_is_ready(tamper_spec.port)) {
		LOG_ERR("Port not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&tamper_spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	static struct ctr_edge edge;

	ret = ctr_edge_init(&edge, &tamper_spec, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(&edge, edge_callback, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(&edge, 10);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(&edge, 50);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(&edge, 50);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_watch(&edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);
		return ret;
	}

	return 0;
}

int app_tamper_clear(void)
{
	app_data_lock();
	g_app_data.tamper.event_count = 0;
	app_data_unlock();

	return 0;
}
