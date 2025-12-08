/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/ctr_button.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_edge.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_x0.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

#if defined(CONFIG_SHIELD_CTR_LTE)
K_SEM_DEFINE(g_app_init_sem, 0, 1);
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(CONFIG_SHIELD_CTR_Z)

static int init_chester_z(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_z_set_handler(dev, app_handler_ctr_z, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
		return ret;
	}

	ret = ctr_z_enable_interrupts(dev);
	if (ret) {
		LOG_ERR("Call `ctr_z_enable_interrupts` failed: %d", ret);
		return ret;
	}

	uint32_t serial_number;
	ret = ctr_z_get_serial_number(dev, &serial_number);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_serial_number` failed: %d", ret);
		return ret;
	}

	LOG_INF("Serial number: %08x", serial_number);

	uint16_t hw_revision;
	ret = ctr_z_get_hw_revision(dev, &hw_revision);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_revision` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW revision: %04x", hw_revision);

	uint32_t hw_variant;
	ret = ctr_z_get_hw_variant(dev, &hw_variant);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_hw_variant` failed: %d", ret);
		return ret;
	}

	LOG_INF("HW variant: %08x", hw_variant);

	uint32_t fw_version;
	ret = ctr_z_get_fw_version(dev, &fw_version);
	if (ret) {
		LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
		return ret;
	}

	LOG_INF("FW version: %08x", fw_version);

	char vendor_name[32];
	ret = ctr_z_get_vendor_name(dev, vendor_name, sizeof(vendor_name));
	if (ret) {
		LOG_ERR("Call `ctr_z_get_vendor_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Vendor name: %s", vendor_name);

	char product_name[32];
	ret = ctr_z_get_product_name(dev, product_name, sizeof(product_name));
	if (ret) {
		LOG_ERR("Call `ctr_z_get_product_name` failed: %d", ret);
		return ret;
	}

	LOG_INF("Product name: %s", product_name);

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)

static int init_edge(struct ctr_edge *edge, ctr_edge_cb_t cb, const struct device *dev,
		     enum ctr_x0_channel channel, enum ctr_x0_mode mode, int active_duration,
		     int inactive_duration, int cooldown_time)
{
	int ret;

	/* Call ctr_x0_set_mode only in NPN mode. In PNP we will use MCU internal pull-down to get
	 * lower analog threshold for GPIO interrupt (21 V vs 15.5 V) */
	if (mode == CTR_X0_MODE_NPN_INPUT) {
		ret = ctr_x0_set_mode(dev, channel, mode);
		if (ret) {
			LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
			return ret;
		}
	}

	const struct gpio_dt_spec *spec;
	ret = ctr_x0_get_spec(dev, channel, &spec);
	if (ret) {
		LOG_ERR("Call `ctr_x0_get_spec` failed: %d", ret);
		return ret;
	}

	if (mode == CTR_X0_MODE_NPN_INPUT) {
		ret = gpio_pin_configure_dt(spec, GPIO_INPUT | GPIO_ACTIVE_LOW);
		if (ret) {
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
			return ret;
		}
	} else {
		/* Apply pull-down on MCU instead of X0 to get lower analog threshold for GPIO
		 * interrupt */
		ret = gpio_pin_configure_dt(spec, GPIO_INPUT | GPIO_ACTIVE_HIGH | GPIO_PULL_DOWN);
		if (ret) {
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
			return ret;
		}
	}

	ret = ctr_edge_init(edge, spec, false);
	if (ret) {
		LOG_ERR("Call `ctr_edge_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_callback(edge, cb, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_callback` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_cooldown_time(edge, cooldown_time);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_cooldown_time` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_active_duration(edge, active_duration);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_active_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_set_inactive_duration(edge, inactive_duration);
	if (ret) {
		LOG_ERR("Call `ctr_edge_set_inactive_duration` failed: %d", ret);
		return ret;
	}

	ret = ctr_edge_watch(edge);
	if (ret) {
		LOG_ERR("Call `ctr_edge_watch` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init_chester_x0(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	if (g_app_config.trigger_input_type != APP_CONFIG_INPUT_TYPE_NPN &&
	    g_app_config.trigger_input_type != APP_CONFIG_INPUT_TYPE_PNP) {
		LOG_ERR("Unknown trigger input type");
		return -EINVAL;
	}

	enum ctr_x0_mode trigger_mode = g_app_config.trigger_input_type == APP_CONFIG_INPUT_TYPE_NPN
						? CTR_X0_MODE_NPN_INPUT
						: CTR_X0_MODE_PNP_INPUT;

	/* Channel 1: Trigger input */
	static struct ctr_edge edge_trigger;
	ret = init_edge(&edge_trigger, app_handler_edge_trigger_callback, dev, CTR_X0_CHANNEL_1,
			trigger_mode, g_app_config.trigger_duration_active,
			g_app_config.trigger_duration_inactive, g_app_config.trigger_cooldown_time);
	if (ret) {
		LOG_ERR("Call `init_edge` failed: %d", ret);
		return ret;
	}

	if (g_app_config.counter_input_type != APP_CONFIG_INPUT_TYPE_NPN &&
	    g_app_config.counter_input_type != APP_CONFIG_INPUT_TYPE_PNP) {
		LOG_ERR("Unknown counter input type");
		return -EINVAL;
	}

	enum ctr_x0_mode counter_mode = g_app_config.counter_input_type == APP_CONFIG_INPUT_TYPE_NPN
						? CTR_X0_MODE_NPN_INPUT
						: CTR_X0_MODE_PNP_INPUT;

	/* Channel 2: Counter input */
	static struct ctr_edge edge_counter;
	ret = init_edge(&edge_counter, app_handler_edge_counter_callback, dev, CTR_X0_CHANNEL_2,
			counter_mode, g_app_config.counter_duration_active,
			g_app_config.counter_duration_inactive, g_app_config.counter_cooldown_time);
	if (ret) {
		LOG_ERR("Call `init_edge` failed: %d", ret);
		return ret;
	}

	/* Channel 3: Analog voltage input (0-10 V) */
	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_3, CTR_X0_MODE_AI_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A2);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	/* Channel 4: Current loop input (4-20 mA) */
	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_4, CTR_X0_MODE_CL_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A3);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	return 0;
}

#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

int app_init(void)
{
	int ret;

	ctr_led_set(CTR_LED_CHANNEL_R, true);

	ret = ctr_wdog_set_timeout(120000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_install(&g_app_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		return ret;
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_CTR_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_CTR_BUTTON) */

#if defined(CONFIG_SHIELD_CTR_LTE)
	ret = ctr_lte_set_event_cb(app_handler_lte, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_set_event_cb` failed: %d", ret);
		return ret;
	}

	ret = ctr_lte_start(NULL);
	if (ret) {
		LOG_ERR("Call `ctr_lte_start` failed: %d", ret);
		return ret;
	}

	for (;;) {
		ret = ctr_wdog_feed(&g_app_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			return ret;
		}

		ret = k_sem_take(&g_app_init_sem, K_SECONDS(1));
		if (ret == -EAGAIN) {
			continue;
		} else if (ret) {
			LOG_ERR("Call `k_sem_take` failed: %d", ret);
			return ret;
		}

		break;
	}
#endif /* defined(CONFIG_SHIELD_CTR_LTE) */

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_SHIELD_CTR_Z)
	ret = init_chester_z();
	if (ret) {
		LOG_ERR("Call `init_chester_z` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_Z) */

#if defined(CONFIG_SHIELD_CTR_X0_A)
	ret = init_chester_x0();
	if (ret) {
		LOG_ERR("Call `init_chester_x0` failed: %d", ret);
		k_oops();
	}
#endif /* defined(CONFIG_SHIELD_CTR_X0_A) */

#if defined(CONFIG_SHIELD_CTR_DS18B20)
	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHIELD_CTR_DS18B20) */

	return 0;
}
