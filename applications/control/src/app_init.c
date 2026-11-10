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
#include "app_sensor_drv.h"
#include "app_work.h"
#include "app_x0_map.h"
#include "feature.h"

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/ctr_button.h>
#include <chester/ctr_cloud.h>
#include <chester/ctr_soil_sensor.h>
#include <chester/ctr_ds18b20.h>
#include <chester/ctr_edge.h>
#include <chester/ctr_led.h>
#include <chester/ctr_lrw.h>
#include <chester/ctr_lte.h>
#include <chester/ctr_wdog.h>
#include <chester/drivers/ctr_x0.h>

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

/* X0 channel/ADC maps live in app_x0_map.h (single definition). */

static int init_chester_x0_triggers(const struct device *dev, int ch_start, int ch_count,
				    enum app_config_channel_mode *modes)
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

	for (int i = 0; i < ch_count; i++) {
		int ch_idx = ch_start + i;

		if (modes[i] == APP_CONFIG_CHANNEL_MODE_TRIGGER) {
			LOG_INF("Initialize trigger channel %d", ch_idx);

			/* Alloc memory */
			g_app_data.trigger[ch_idx] = k_malloc(sizeof(struct app_data_trigger));
			if (!g_app_data.trigger[ch_idx]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.trigger[ch_idx], 0, sizeof(struct app_data_trigger));

			/* Initialize channel */
			ret = init_edge(&g_app_data.trigger[ch_idx]->edge,
					app_handler_edge_trigger_callback, dev,
					app_x0_channel_map[ch_idx], trigger_mode,
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

static int init_chester_x0_counters(const struct device *dev, int ch_start, int ch_count,
				    enum app_config_channel_mode *modes)
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

	for (int i = 0; i < ch_count; i++) {
		int ch_idx = ch_start + i;

		if (modes[i] == APP_CONFIG_CHANNEL_MODE_COUNTER) {
			LOG_INF("Initialize counter channel %d", ch_idx);

			/* Alloc memory */
			g_app_data.counter[ch_idx] = k_malloc(sizeof(struct app_data_counter));
			if (!g_app_data.counter[ch_idx]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.counter[ch_idx], 0, sizeof(struct app_data_counter));

			/* Initialize channel */
			ret = init_edge(&g_app_data.counter[ch_idx]->edge,
					app_handler_edge_counter_callback, dev,
					app_x0_channel_map[ch_idx], counter_mode,
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

static int init_chester_x0_voltages(const struct device *dev, int ch_start, int ch_count,
				    enum app_config_channel_mode *modes)
{
	int ret;

	LOG_INF("app_data_analog struct size %d", sizeof(struct app_data_analog));

	for (int i = 0; i < ch_count; i++) {
		int ch_idx = ch_start + i;

		if (modes[i] == APP_CONFIG_CHANNEL_MODE_VOLTAGE) {
			LOG_INF("Initialize voltage channel %d", ch_idx);

			/* Alloc memory */
			g_app_data.voltage[ch_idx] = k_malloc(sizeof(struct app_data_analog));
			if (!g_app_data.voltage[ch_idx]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.voltage[ch_idx], 0, sizeof(struct app_data_analog));
			g_app_data.voltage[ch_idx]->last_sample = NAN;

			/* Initialize channel */
			ret = ctr_x0_set_mode(dev, app_x0_channel_map[ch_idx],
					      CTR_X0_MODE_AI_INPUT);
			if (ret) {
				LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
				return ret;
			}

			ret = ctr_adc_init(app_x0_adc_map[ch_idx]);
			if (ret) {
				LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
				return ret;
			}
		}
	}

	return 0;
}

static int init_chester_x0_currents(const struct device *dev, int ch_start, int ch_count,
				    enum app_config_channel_mode *modes)
{
	int ret;

	LOG_INF("app_data_analog struct size %d", sizeof(struct app_data_analog));

	for (int i = 0; i < ch_count; i++) {
		int ch_idx = ch_start + i;

		if (modes[i] == APP_CONFIG_CHANNEL_MODE_CURRENT) {
			LOG_INF("Initialize current channel %d", ch_idx);

			/* Alloc memory */
			g_app_data.current[ch_idx] = k_malloc(sizeof(struct app_data_analog));
			if (!g_app_data.current[ch_idx]) {
				LOG_ERR("Call `k_malloc` failed");
				return -ENOMEM;
			}
			memset(g_app_data.current[ch_idx], 0, sizeof(struct app_data_analog));
			g_app_data.current[ch_idx]->last_sample = NAN;

			/* Initialize channel */
			ret = ctr_x0_set_mode(dev, app_x0_channel_map[ch_idx],
					      CTR_X0_MODE_CL_INPUT);
			if (ret) {
				LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
				return ret;
			}

			ret = ctr_adc_init(app_x0_adc_map[ch_idx]);
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

	enum app_config_channel_mode modes_a[] = {
		g_app_config.channel_mode_1,
		g_app_config.channel_mode_2,
		g_app_config.channel_mode_3,
		g_app_config.channel_mode_4,
	};

	/* Modes are derived from sensor ownership, in both directions, whatever
	 * channel-mode-N says. The two roles a sensor claims are reported
	 * differently so the pair describes itself: the supply channel as `power`
	 * (no payload) and the measure channel as `voltage`. The measure channel's
	 * value stays NaN — the reading is sent in the SENSORS group rather than
	 * duplicated here — and `app_sensor_voltage_sample` skips owned channels, so
	 * there is still no double sampling.
	 *
	 * A channel set to `power` by hand that no sensor claims becomes `disabled`,
	 * because nothing drives it. A hand-set `voltage` is left alone: an ordinary
	 * voltage channel with no sensor is a legitimate configuration. */
	for (int i = 0; i < 4; i++) {
		if (app_config_channel_owner(i + 1) >= 0) {
			modes_a[i] = APP_CONFIG_CHANNEL_MODE_POWER;
		} else if (modes_a[i] == APP_CONFIG_CHANNEL_MODE_POWER) {
			LOG_WRN("Channel %d: mode `power` but no sensor claims it; "
				"treating as disabled",
				i + 1);
			modes_a[i] = APP_CONFIG_CHANNEL_MODE_DISABLED;
		}
	}

	for (int i = 0; i < 4; i++) {
		/* `power` channels are owned by the sensor layer: park them in the
		 * same safe default state as a disabled channel and allocate no
		 * trigger/counter/voltage/current record, so the raw X0 path never
		 * samples them. `app_sensor_drv_init` runs later and re-drives them
		 * per sample. */
		if (modes_a[i] == APP_CONFIG_CHANNEL_MODE_DISABLED ||
		    modes_a[i] == APP_CONFIG_CHANNEL_MODE_POWER) {
			int owner = app_config_channel_owner(i + 1);
			bool power = modes_a[i] == APP_CONFIG_CHANNEL_MODE_POWER;
			bool is_supply = owner >= 0 && g_app_config.sensors[owner].ch_power == i + 1;

			LOG_INF("Channel %d %s", i + 1, power ? "driven by sensor" : "disabled");

			/* Report the two roles a sensor claims differently, so the pair
			 * describes itself: the supply channel as `power` (no payload)
			 * and the measure channel as `voltage`.
			 *
			 * The measure channel gets its analog record allocated here
			 * rather than via `init_chester_x0_voltages`, deliberately: that
			 * helper would also switch the pin to AI_INPUT permanently and
			 * re-run `ctr_adc_init` on a channel the sensor layer already
			 * owns and initialises. Both are wrong here — the node must be
			 * left alone between samples, and the sensor drives and releases
			 * the channel itself. `last_sample` stays NaN so the encoder emits
			 * the invalid marker and the real reading travels in the SENSORS
			 * group instead of being duplicated. */
			if (power && !is_supply) {
				g_app_data.voltage[i] = k_malloc(sizeof(struct app_data_analog));
				if (!g_app_data.voltage[i]) {
					LOG_ERR("Call `k_malloc` failed");
					return -ENOMEM;
				}
				memset(g_app_data.voltage[i], 0, sizeof(struct app_data_analog));
				g_app_data.voltage[i]->last_sample = NAN;
			} else {
				g_app_data.power[i] = power;
			}

			ret = ctr_x0_set_mode(dev, app_x0_channel_map[i],
					      CTR_X0_MODE_DEFAULT);
			if (ret) {
				LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
				return ret;
			}
		}
	}

	ret = init_chester_x0_triggers(dev, 0, 4, modes_a);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_triggers` failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_counters(dev, 0, 4, modes_a);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_counters` failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_voltages(dev, 0, 4, modes_a);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_voltages` failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_currents(dev, 0, 4, modes_a);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_currents` failed: %d", ret);
		return ret;
	}

	/* Clear spurious events from GPIO initialization */
	for (int i = 0; i < 4; i++) {
		if (g_app_data.trigger[i]) {
			g_app_data.trigger[i]->event_count = 0;
		}
		if (g_app_data.counter[i]) {
			g_app_data.counter[i]->value = 0;
			g_app_data.counter[i]->delta = 0;
		}
	}

	print_heap_stats();

	return 0;
}

#if defined(FEATURE_HARDWARE_CHESTER_X0_B)

static int init_chester_x0_b(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_b));

	if (!device_is_ready(dev)) {
		LOG_ERR("Module X0_B not found (hardware not present)");
		return 0;
	}

	print_heap_stats();

	enum app_config_channel_mode modes_b[] = {
		g_app_config.channel_mode_5,
		g_app_config.channel_mode_6,
		g_app_config.channel_mode_7,
		g_app_config.channel_mode_8,
	};

	/* See `init_chester_x0` — modes are derived from sensor ownership both ways,
	 * supply channel as `power` and measure channel as `voltage`. */
	for (int i = 0; i < 4; i++) {
		int owner = app_config_channel_owner(i + 5);

		if (owner >= 0) {
			modes_b[i] = APP_CONFIG_CHANNEL_MODE_POWER;
		} else if (modes_b[i] == APP_CONFIG_CHANNEL_MODE_POWER) {
			LOG_WRN("Channel %d: mode `power` but no sensor claims it; "
				"treating as disabled",
				i + 5);
			modes_b[i] = APP_CONFIG_CHANNEL_MODE_DISABLED;
		}
	}

	for (int i = 0; i < 4; i++) {
		/* See `init_chester_x0` — `power` channels belong to the sensor
		 * layer. Note the X0_B channel map is indexed 0-3 per device, while
		 * `g_app_data.power` is indexed by global channel number. */
		if (modes_b[i] == APP_CONFIG_CHANNEL_MODE_DISABLED ||
		    modes_b[i] == APP_CONFIG_CHANNEL_MODE_POWER) {
			int owner = app_config_channel_owner(i + 5);
			bool power = modes_b[i] == APP_CONFIG_CHANNEL_MODE_POWER;
			bool is_supply = owner >= 0 && g_app_config.sensors[owner].ch_power == i + 5;

			LOG_INF("Channel %d %s", i + 5, power ? "driven by sensor" : "disabled");

			/* See `init_chester_x0` — supply channel reports as `power`, the
			 * measure channel as `voltage` with no value. */
			if (power && !is_supply) {
				g_app_data.voltage[i + 4] = k_malloc(sizeof(struct app_data_analog));
				if (!g_app_data.voltage[i + 4]) {
					LOG_ERR("Call `k_malloc` failed");
					return -ENOMEM;
				}
				memset(g_app_data.voltage[i + 4], 0, sizeof(struct app_data_analog));
				g_app_data.voltage[i + 4]->last_sample = NAN;
			} else {
				g_app_data.power[i + 4] = power;
			}

			ret = ctr_x0_set_mode(dev, app_x0_channel_map[i],
					      CTR_X0_MODE_DEFAULT);
			if (ret) {
				LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
				return ret;
			}
		}
	}

	ret = init_chester_x0_triggers(dev, 4, 4, modes_b);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_triggers` (B) failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_counters(dev, 4, 4, modes_b);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_counters` (B) failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_voltages(dev, 4, 4, modes_b);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_voltages` (B) failed: %d", ret);
		return ret;
	}

	ret = init_chester_x0_currents(dev, 4, 4, modes_b);
	if (ret) {
		LOG_ERR("Call `init_chester_x0_currents` (B) failed: %d", ret);
		return ret;
	}

	/* Clear spurious events from GPIO initialization */
	for (int i = 4; i < 8; i++) {
		if (g_app_data.trigger[i]) {
			g_app_data.trigger[i]->event_count = 0;
		}
		if (g_app_data.counter[i]) {
			g_app_data.counter[i]->value = 0;
			g_app_data.counter[i]->delta = 0;
		}
	}

	print_heap_stats();

	return 0;
}

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_B) */

#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
static int init_soil_sensor(void)
{
	int ret;

	ret = ctr_soil_sensor_scan();
	if (ret) {
		LOG_ERR("Call `ctr_soil_sensor_scan` failed: %d", ret);
		return ret;
	}

	int count = ctr_soil_sensor_get_count();

	/*
	 * The soil sensor array is heap-allocated (~1.3 KB per sensor) from the
	 * shared system heap (CONFIG_HEAP_MEM_POOL_SIZE), which other subsystems
	 * (e.g. the X0_A channels) also draw from. Cap the number we track so a
	 * user connecting many sensors cannot exhaust the heap and starve the
	 * rest of the application. Raise APP_DATA_SOIL_SENSOR_MAX_COUNT only
	 * after confirming the heap budget can absorb the extra sensors.
	 */
	if (count > APP_DATA_SOIL_SENSOR_MAX_COUNT) {
		LOG_WRN("Detected %d soil sensors, limiting to %d", count,
			APP_DATA_SOIL_SENSOR_MAX_COUNT);
		count = APP_DATA_SOIL_SENSOR_MAX_COUNT;
	}

	if (count > 0) {
		size_t size = count * sizeof(struct app_data_soil_sensor_sensor);

		g_app_data.soil_sensor.sensor = k_malloc(size);
		if (!g_app_data.soil_sensor.sensor) {
			LOG_ERR("Call `k_malloc` failed");
			return -ENOMEM;
		}
		memset(g_app_data.soil_sensor.sensor, 0, size);
	}

	g_app_data.soil_sensor.sensor_count = count;

	return 0;
}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
static int init_w1_therm(void)
{
	int ret;

	ret = ctr_ds18b20_scan();
	if (ret) {
		LOG_ERR("Call `ctr_ds18b20_scan` failed: %d", ret);
		return ret;
	}

	int count = ctr_ds18b20_get_count();

	/*
	 * The w1_therm array is heap-allocated (~1.2 KB per sensor) from the
	 * shared system heap (CONFIG_HEAP_MEM_POOL_SIZE), which other subsystems
	 * also draw from. Cap the number we track so a user connecting many
	 * sensors cannot exhaust the heap and starve the rest of the
	 * application. Raise APP_DATA_W1_THERM_MAX_COUNT only after confirming
	 * the heap budget can absorb the extra sensors.
	 */
	if (count > APP_DATA_W1_THERM_MAX_COUNT) {
		LOG_WRN("Detected %d w1 thermometers, limiting to %d", count,
			APP_DATA_W1_THERM_MAX_COUNT);
		count = APP_DATA_W1_THERM_MAX_COUNT;
	}

	if (count > 0) {
		size_t size = count * sizeof(struct app_data_w1_therm_sensor);

		g_app_data.w1_therm.sensor = k_malloc(size);
		if (!g_app_data.w1_therm.sensor) {
			LOG_ERR("Call `k_malloc` failed");
			return -ENOMEM;
		}
		memset(g_app_data.w1_therm.sensor, 0, size);

		for (int i = 0; i < count; i++) {
			g_app_data.w1_therm.sensor[i].last_sample_temperature = NAN;
		}
	}

	g_app_data.w1_therm.sensor_count = count;

	return 0;
}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

int app_init(void)
{
	int ret;

	ret = app_data_init();
	if (ret) {
		LOG_ERR("Call `app_data_init` failed: %d", ret);
		return ret;
	}

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

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	ret = init_chester_x0();
	if (ret) {
		LOG_ERR("Call `init_chester_x0` failed: %d", ret);
	}

#if defined(FEATURE_HARDWARE_CHESTER_X0_B)
	ret = init_chester_x0_b();
	if (ret) {
		LOG_ERR("Call `init_chester_x0_b` failed: %d", ret);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_B) */
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

#if defined(FEATURE_SUBSYSTEM_DS18B20)
	ret = init_w1_therm();
	if (ret) {
		LOG_ERR("Call `init_w1_therm` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_DS18B20) */

#if defined(FEATURE_SUBSYSTEM_SOIL_SENSOR)
	ret = init_soil_sensor();
	if (ret) {
		LOG_ERR("Call `init_soil_sensor` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_SOIL_SENSOR) */

#if defined(FEATURE_SUBSYSTEM_LRW)
	if (g_app_config.mode == APP_CONFIG_MODE_LRW) {
		ret = ctr_lrw_init(app_handler_lrw, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_init` failed: %d", ret);
			return ret;
		}

		ret = ctr_lrw_start(NULL);
		if (ret) {
			LOG_ERR("Call `ctr_lrw_start` failed: %d", ret);
			return ret;
		}

		k_sleep(K_SECONDS(2));
	}
#endif /* defined(FEATURE_SUBSYSTEM_LRW) */

#if defined(FEATURE_SUBSYSTEM_CLOUD)
	if (g_app_config.mode == APP_CONFIG_MODE_LTE) {
		CODEC_CLOUD_OPTIONS_STATIC(copt);

		ret = ctr_cloud_init(&copt);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_init` failed: %d", ret);
			return ret;
		}

		ret = ctr_cloud_set_callback(app_handler_cloud_event, NULL);
		if (ret) {
			LOG_ERR("Call `ctr_cloud_set_callback` failed: %d", ret);
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

#if defined(FEATURE_SUBSYSTEM_POWER)
	ret = ctr_power_init(app_handler_power_event, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_power_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(FEATURE_SUBSYSTEM_POWER) */

#if defined(FEATURE_HARDWARE_CHESTER_X0_A)
	/* Non-fatal: init all configured pluggable sensors (LD-81 etc.). A sensor
	 * problem must never panic the device (temps + LRW keep running). */
	ret = app_sensor_drv_init();
	if (ret) {
		LOG_WRN("Call `app_sensor_drv_init` failed: %d", ret);
	}
#endif /* defined(FEATURE_HARDWARE_CHESTER_X0_A) */

	ret = app_work_init();
	if (ret) {
		LOG_ERR("Call `app_work_init` failed: %d", ret);
		return ret;
	}

	return 0;
}
