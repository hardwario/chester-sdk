/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"

/* CHESTER includes */
#include <chester/drivers/ctr_s1.h>
#include <chester/drivers/ctr_x10.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_iaq, LOG_LEVEL_DBG);

static const struct device *dev_s1 = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

enum iaq_led_state {
	IAQ_LED_STATE_UNKNOWN = 0,
	IAQ_LED_STATE_GOOD = 1,
	IAQ_LED_STATE_WARNING = 2,
	IAQ_LED_STATE_ALARM = 3,
};

enum iaq_led_state led_state = IAQ_LED_STATE_UNKNOWN;

static int led_set(enum iaq_led_state state, bool steady_on)
{
	int ret;

	struct ctr_s1_led_param param_r = {.brightness = CTR_S1_LED_BRIGHTNESS_MEDIUM,
					   .command = CTR_S1_LED_COMMAND_NONE,
					   .pattern = CTR_S1_LED_PATTERN_OFF};

	struct ctr_s1_led_param param_g = {.brightness = CTR_S1_LED_BRIGHTNESS_MEDIUM,
					   .command = CTR_S1_LED_COMMAND_NONE,
					   .pattern = CTR_S1_LED_PATTERN_OFF};

	switch (state) {
	case IAQ_LED_STATE_UNKNOWN:
		break;

	case IAQ_LED_STATE_GOOD:
		if (steady_on) {
			param_g.command = CTR_S1_LED_COMMAND_NONE;
			param_g.pattern = CTR_S1_LED_PATTERN_ON;
		} else {
			param_g.command = CTR_S1_LED_COMMAND_1X_1_16;
			param_g.pattern = CTR_S1_LED_PATTERN_OFF;
		}
		break;

	case IAQ_LED_STATE_WARNING:
		if (steady_on) {
			param_g.brightness = CTR_S1_LED_BRIGHTNESS_LOW;
			param_g.command = CTR_S1_LED_COMMAND_NONE;
			param_g.pattern = CTR_S1_LED_PATTERN_ON;

			param_r.command = CTR_S1_LED_COMMAND_NONE;
			param_r.pattern = CTR_S1_LED_PATTERN_ON;
		} else {
			param_g.brightness = CTR_S1_LED_BRIGHTNESS_LOW;
			param_g.command = CTR_S1_LED_COMMAND_1X_1_16;
			param_g.pattern = CTR_S1_LED_PATTERN_OFF;

			param_r.command = CTR_S1_LED_COMMAND_1X_1_16;
			param_r.pattern = CTR_S1_LED_PATTERN_OFF;
		}
		break;

	case IAQ_LED_STATE_ALARM:
		if (steady_on) {
			param_r.command = CTR_S1_LED_COMMAND_NONE;
			param_r.pattern = CTR_S1_LED_PATTERN_ON;
		} else {
			param_r.command = CTR_S1_LED_COMMAND_1X_1_16;
			param_r.pattern = CTR_S1_LED_PATTERN_OFF;
		}
		break;

	default:
		break;
	}

	ret = ctr_s1_set_led(dev_s1, CTR_S1_LED_CHANNEL_R, &param_r);
	if (ret) {
		LOG_ERR("Call `ctr_s1_set_led` failed: %d", ret);
		return ret;
	}

	ret = ctr_s1_set_led(dev_s1, CTR_S1_LED_CHANNEL_G, &param_g);
	if (ret) {
		LOG_ERR("Call `ctr_s1_set_led` failed: %d", ret);
		return ret;
	}

	ret = ctr_s1_apply(dev_s1);
	if (ret) {
		LOG_ERR("Call `ctr_s1_apply` failed: %d", ret);
		return ret;
	}

	return 0;
}

extern struct app_config m_app_config_interim;

int app_iaq_led_task(void)
{
	__unused int ret;

	if (!device_is_ready(dev_s1)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	float value = g_app_data.iaq.sensors.last_co2_conc;

	/* Hysteresis */
	switch (led_state) {
	case IAQ_LED_STATE_UNKNOWN:
		if (value <= g_app_config.iaq_led_thr_warning) {
			led_state = IAQ_LED_STATE_GOOD;
		} else if (value >= g_app_config.iaq_led_thr_alarm) {
			led_state = IAQ_LED_STATE_ALARM;
		} else {
			led_state = IAQ_LED_STATE_WARNING;
		}
		break;

	case IAQ_LED_STATE_GOOD:
		if (value >= g_app_config.iaq_led_thr_alarm + g_app_config.iaq_led_hst) {
			led_state = IAQ_LED_STATE_ALARM;
		} else if (value >= g_app_config.iaq_led_thr_warning + g_app_config.iaq_led_hst) {
			led_state = IAQ_LED_STATE_WARNING;
		}
		break;

	case IAQ_LED_STATE_WARNING:
		if (value >= g_app_config.iaq_led_thr_alarm + g_app_config.iaq_led_hst) {
			led_state = IAQ_LED_STATE_ALARM;
		} else if (value <= g_app_config.iaq_led_thr_warning - g_app_config.iaq_led_hst) {
			led_state = IAQ_LED_STATE_GOOD;
		}
		break;

	case IAQ_LED_STATE_ALARM:
		if (value <= g_app_config.iaq_led_thr_warning - g_app_config.iaq_led_hst) {
			led_state = IAQ_LED_STATE_GOOD;
		} else if (value <= g_app_config.iaq_led_thr_alarm - g_app_config.iaq_led_hst) {
			led_state = IAQ_LED_STATE_WARNING;
		}
		break;
	}

	bool line_present = false;

#if defined(CONFIG_SHIELD_CTR_X10)
	const struct device *dev_x10 = DEVICE_DT_GET(DT_NODELABEL(ctr_x10));

	ret = ctr_x10_get_line_present(dev_x10, &line_present);
	if (ret) {
		LOG_ERR("Call `ctr_x10_get_line_present` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_X10) */

	led_set(led_state, line_present);

	return 0;
}
