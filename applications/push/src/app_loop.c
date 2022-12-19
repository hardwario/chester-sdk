#include "app_data.h"
#include "app_loop.h"
#include "app_measure.h"
#include "app_send.h"

/* CHESTER includes */
#include <chester/ctr_accel.h>
#include <chester/ctr_hygro.h>
#include <chester/ctr_led.h>
#include <chester/ctr_therm.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_batt.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_loop, LOG_LEVEL_DBG);

#define BATT_TEST_INTERVAL_MSEC (12 * 60 * 60 * 1000)

K_SEM_DEFINE(g_app_loop_sem, 1, 1);
atomic_t g_app_loop_measure = true;
atomic_t g_app_loop_send = true;

extern struct ctr_wdog_channel g_app_wdog_channel;

static int task_battery(void)
{
	int ret = 0;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_batt));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	static int64_t next;

	if (k_uptime_get() >= next) {
		int voltage_rest_mv;
		ret = ctr_batt_get_rest_voltage_mv(dev, &voltage_rest_mv,
						   CTR_BATT_REST_TIMEOUT_DEFAULT_MS);
		if (ret) {
			LOG_ERR("Call `ctr_batt_get_rest_voltage_mv` failed: %d", ret);
			goto error;
		}

		int voltage_load_mv;
		ret = ctr_batt_get_load_voltage_mv(dev, &voltage_load_mv,
						   CTR_BATT_LOAD_TIMEOUT_DEFAULT_MS);
		if (ret) {
			LOG_ERR("Call `ctr_batt_get_load_voltage_mv` failed: %d", ret);
			goto error;
		}

		int current_load_ma;
		ctr_batt_get_load_current_ma(dev, &current_load_ma, voltage_load_mv);

		LOG_INF("Battery rest voltage: %u mV", voltage_rest_mv);
		LOG_INF("Battery load voltage: %u mV", voltage_load_mv);
		LOG_INF("Battery load current: %u mA", current_load_ma);

		next = k_uptime_get() + BATT_TEST_INTERVAL_MSEC;

		g_app_data.errors.batt_voltage_rest = false;
		g_app_data.errors.batt_voltage_load = false;
		g_app_data.errors.batt_current_load = false;

		g_app_data.states.batt_voltage_rest = voltage_rest_mv / 1000.f;
		g_app_data.states.batt_voltage_load = voltage_load_mv / 1000.f;
		g_app_data.states.batt_current_load = current_load_ma;
	}

	return 0;

error:
	g_app_data.errors.batt_voltage_rest = true;
	g_app_data.errors.batt_voltage_load = true;
	g_app_data.errors.batt_current_load = true;

	return ret;
}

static int task_chester_z(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		ret = -ENODEV;
		goto error;
	}

	struct ctr_z_status status;
	ret = ctr_z_get_status(dev, &status);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_status` failed: %d", ret);
		goto error;
	}

	LOG_INF("DC input connected: %d", (int)status.dc_input_connected);

	uint16_t vdc;
	ret = ctr_z_get_vdc_mv(dev, &vdc);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vdc_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage DC input: %u mV", vdc);

	uint16_t vbat;
	ret = ctr_z_get_vbat_mv(dev, &vbat);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vbat_mv` failed: %d", ret);
		goto error;
	}

	LOG_INF("Voltage backup battery: %u mV", vbat);

	g_app_data.errors.line_present = false;
	g_app_data.errors.line_voltage = false;
	g_app_data.errors.bckp_voltage = false;

	g_app_data.states.line_present = status.dc_input_connected;
	g_app_data.states.line_voltage = vdc / 1000.f;
	g_app_data.states.bckp_voltage = vbat / 1000.f;

	return 0;

error:
	g_app_data.errors.line_present = true;
	g_app_data.errors.line_voltage = true;
	g_app_data.errors.bckp_voltage = true;

	return ret;
}

int app_loop(void)
{
	int ret;

	ret = ctr_wdog_feed(&g_app_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
		return ret;
	}

	ctr_led_set(CTR_LED_CHANNEL_G, true);
	k_sleep(K_MSEC(30));
	ctr_led_set(CTR_LED_CHANNEL_G, false);

	ret = k_sem_take(&g_app_loop_sem, K_SECONDS(5));
	if (ret == -EAGAIN) {
		return 0;
	} else if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		return ret;
	}

	bool is_send = atomic_clear(&g_app_loop_send);

	ret = task_battery();
	if (ret) {
		LOG_ERR("Call `task_battery` failed: %d", ret);
		return ret;
	}

	ret = task_chester_z();
	if (ret) {
		LOG_ERR("Call `task_chester_z` failed: %d", ret);
		return ret;
	}

	if (atomic_set(&g_app_loop_measure, false)) {
		ret = app_measure();
		if (ret) {
			LOG_ERR("Call `app_measure` failed: %d", ret);
			return ret;
		}
	}

	if (is_send) {
		ret = app_send();
		if (ret) {
			LOG_ERR("Call `app_send` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}
