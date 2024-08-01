/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "app_codec.h"
#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_init.h"
#include "app_work.h"

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>
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
#include <zephyr/sys/sys_heap.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(app_init, LOG_LEVEL_DBG);

struct ctr_wdog_channel g_app_wdog_channel;

#if defined(FEATURE_HARDWARE_CHESTER_Z)

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

#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)

static int init_edge(struct ctr_edge *edge, ctr_edge_cb_t cb, const struct device *dev,
		     enum ctr_x0_channel channel, enum ctr_x0_mode mode, int active_duration,
		     int inactive_duration, int cooldown_time, void *user_data)
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

	ret = ctr_edge_set_callback(edge, cb, user_data);
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

static enum ctr_x0_channel x0_channel_lookup[] = {CTR_X0_CHANNEL_1, CTR_X0_CHANNEL_2,
						  CTR_X0_CHANNEL_3, CTR_X0_CHANNEL_4};

static enum ctr_x0_channel x0_adc_lookup[] = {CTR_ADC_CHANNEL_A0, CTR_ADC_CHANNEL_A1,
					      CTR_ADC_CHANNEL_A2, CTR_ADC_CHANNEL_A3};

static int init_chester_x0_triggers(const struct device *dev)
{
	int ret;

	LOG_INF("app_data_trigger struct size %d", sizeof(struct app_data_trigger));

	if (g_app_config.trigger_input_type != APP_CONFIG_INPUT_TYPE_NPN &&
	    g_app_config.trigger_input_type != APP_CONFIG_INPUT_TYPE_PNP) {
		LOG_ERR("Unknown trigger input type");
		return -EINVAL;
	}

	enum ctr_x0_mode trigger_mode = g_app_config.trigger_input_type == APP_CONFIG_INPUT_TYPE_NPN
						? CTR_X0_MODE_NPN_INPUT
						: CTR_X0_MODE_PNP_INPUT;

	enum app_config_channel_mode config_channel_mode_lookup[] = {
		g_app_config.channel_mode_1,
		g_app_config.channel_mode_2,
		g_app_config.channel_mode_3,
		g_app_config.channel_mode_4,
	};

	for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {
		if (config_channel_mode_lookup[i] == APP_CONFIG_CHANNEL_MODE_TRIGGER) {
			LOG_INF("Initialize channel %d", i);

			/* Alloc memory */
			g_app_data.trigger[i] = k_malloc(sizeof(struct app_data_trigger));
			if (!g_app_data.trigger[i]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.trigger[i], 0, sizeof(struct app_data_trigger));

			/* Initialize channel */
			ret = init_edge(&g_app_data.trigger[i]->edge,
					app_handler_edge_trigger_callback, dev,
					x0_channel_lookup[i], trigger_mode,
					g_app_config.trigger_duration_active,
					g_app_config.trigger_duration_inactive,
					g_app_config.trigger_cooldown_time, NULL);
			if (ret) {
				LOG_ERR("Call `init_edge` failed: %d", ret);
				return ret;
			}
		}
	}

	return 0;
}

static int init_chester_x0_counters(const struct device *dev)
{
	int ret;

	LOG_INF("app_data_counter struct size %d", sizeof(struct app_data_counter));

	if (g_app_config.counter_input_type != APP_CONFIG_INPUT_TYPE_NPN &&
	    g_app_config.counter_input_type != APP_CONFIG_INPUT_TYPE_PNP) {
		LOG_ERR("Unknown counter input type");
		return -EINVAL;
	}

	enum ctr_x0_mode counter_mode = g_app_config.counter_input_type == APP_CONFIG_INPUT_TYPE_NPN
						? CTR_X0_MODE_NPN_INPUT
						: CTR_X0_MODE_PNP_INPUT;

	enum app_config_channel_mode config_channel_mode_lookup[] = {
		g_app_config.channel_mode_1,
		g_app_config.channel_mode_2,
		g_app_config.channel_mode_3,
		g_app_config.channel_mode_4,
	};

	for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {
		if (config_channel_mode_lookup[i] == APP_CONFIG_CHANNEL_MODE_COUNTER) {
			LOG_INF("Initialize channel %d", i);

			/* Alloc memory */
			g_app_data.counter[i] = k_malloc(sizeof(struct app_data_counter));
			if (!g_app_data.counter[i]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.counter[i], 0, sizeof(struct app_data_counter));

			/* Initialize channel */
			ret = init_edge(&g_app_data.counter[i]->edge,
					app_handler_edge_counter_callback, dev,
					x0_channel_lookup[i], counter_mode,
					g_app_config.counter_duration_active,
					g_app_config.counter_duration_inactive,
					g_app_config.counter_cooldown_time, NULL);
			if (ret) {
				LOG_ERR("Call `init_edge` failed: %d", ret);
				return ret;
			}
		}
	}

	return 0;
}

static int init_chester_x0_voltages(const struct device *dev)
{
	int ret;

	LOG_INF("app_data_analog struct size %d", sizeof(struct app_data_analog));

	enum app_config_channel_mode config_channel_mode_lookup[] = {
		g_app_config.channel_mode_1,
		g_app_config.channel_mode_2,
		g_app_config.channel_mode_3,
		g_app_config.channel_mode_4,
	};

	for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {
		if (config_channel_mode_lookup[i] == APP_CONFIG_CHANNEL_MODE_VOLTAGE) {
			LOG_INF("Initialize channel %d", i);

			/* Alloc memory */
			g_app_data.voltage[i] = k_malloc(sizeof(struct app_data_analog));
			if (!g_app_data.voltage[i]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.voltage[i], 0, sizeof(struct app_data_analog));

			/* Initialize channel */
			ret = ctr_x0_set_mode(dev, x0_channel_lookup[i], CTR_X0_MODE_AI_INPUT);
			if (ret) {
				LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
				return ret;
			}

			ret = ctr_adc_init(x0_adc_lookup[i]);
			if (ret) {
				LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
				return ret;
			}
		}
	}

	return 0;
}

static int init_chester_x0_currents(const struct device *dev)
{
	int ret;

	LOG_INF("app_data_analog struct size %d", sizeof(struct app_data_analog));

	enum app_config_channel_mode config_channel_mode_lookup[] = {
		g_app_config.channel_mode_1,
		g_app_config.channel_mode_2,
		g_app_config.channel_mode_3,
		g_app_config.channel_mode_4,
	};

	for (int i = 0; i < APP_DATA_NUM_CHANNELS; i++) {
		if (config_channel_mode_lookup[i] == APP_CONFIG_CHANNEL_MODE_CURRENT) {
			LOG_INF("Initialize channel %d", i);

			/* Alloc memory */
			g_app_data.current[i] = k_malloc(sizeof(struct app_data_analog));
			if (!g_app_data.current[i]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.current[i], 0, sizeof(struct app_data_analog));

			/* Initialize channel */
			ret = ctr_x0_set_mode(dev, x0_channel_lookup[i], CTR_X0_MODE_CL_INPUT);
			if (ret) {
				LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
				return ret;
			}

			ret = ctr_adc_init(x0_adc_lookup[i]);
			if (ret) {
				LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
				return ret;
			}
		}
	}

	return 0;
}

extern struct sys_heap _system_heap;

static void print_heap_stats(void)
{
	struct sys_memory_stats stats;

	sys_heap_runtime_stats_get(&_system_heap, &stats);

	LOG_INF("allocated %zu, free %zu, max allocated %zu, heap size %u", stats.allocated_bytes,
		stats.free_bytes, stats.max_allocated_bytes, CONFIG_HEAP_MEM_POOL_SIZE);
	/*sys_heap_print_info(&_system_heap, true);*/
}

static int init_chester_x0(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	print_heap_stats();

	ret = init_chester_x0_triggers(dev);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_triggers` failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_counters(dev);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_counters` failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_voltages(dev);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_voltages` failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_currents(dev);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_currents` failed: %d", ret);
		return ret;
	}

	print_heap_stats();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

int app_init(void)
{
	int ret;

#if defined(FEATURE_SUBSYSTEM_BUTTON)
	ret = ctr_button_set_event_cb(app_handler_ctr_button, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_button_set_event_cb` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_BUTTON) */

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

#if defined(FEATURE_SUBSYSTEM_CLOUD)
	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		CODEC_CLOUD_OPTIONS_STATIC(copt);

		ret = ctr_cloud_init(&copt);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
			return ret;
		}

		if (g_app_config.interval_poll > 0) {
			ret = ctr_cloud_set_poll_interval(K_SECONDS(g_app_config.interval_poll));
			if (ret) {
				LOG_ERR("Call `ctr_cloud_set_pull_interval` failed: %d", ret);
				return ret;
			}
		}

		while (true) {
			ret = ctr_cloud_wait_initialized(K_SECONDS(60));
			if (!ret) {
				break;
			} else {
				if (ret == -ETIMEDOUT) {
					LOG_INF("Waiting for cloud initialization");
				} else {
					LOG_ERR("Call `ctr_cloud_wait_initialized` failed: %d",
						ret);
					return ret;
				}
			}

			ret = ctr_wdog_feed(&g_app_wdog_channel);
			if (ret) {
				LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
				return ret;
			}
		}
	}
#endif /* defined(FEATURE_SUBSYSTEM_CLOUD) */

	ctr_led_set(CTR_LED_CHANNEL_R, false);

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

#if defined(FEATURE_HARDWARE_CHESTER_Z)
	ret = init_chester_z();
	if (ret) {
		LOG_ERR("Call `init_chester_z` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_Z) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	ret = init_chester_x0();
	if (ret) {
		LOG_ERR("Call `init_chester_x0` failed: %d", ret);
		k_oops();
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

	return 0;
}
