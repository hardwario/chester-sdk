/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x10.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_x10

LOG_MODULE_REGISTER(ctr_x10, CONFIG_CTR_X10_LOG_LEVEL);

#define R7_KOHM	 330
#define R8_KOHM	 22
#define R33_MOHM 1
#define R35_MOHM 1

struct ctr_x10_config {
	const struct device *adc_dev;
	struct adc_channel_cfg line_adc_channel_cfg;
	struct adc_channel_cfg battery_adc_channel_cfg;
	int line_measurement_interval;
	int line_threshold_min;
	int line_threshold_max;
};

struct ctr_x10_data {
	struct k_mutex lock;
	struct k_timer timer;
	struct k_work work;
	const struct device *dev;
	struct adc_sequence line_adc_sequence;
	struct adc_sequence battery_adc_sequence;
	int16_t adc_buf[1];
	ctr_x10_user_cb user_cb;
	void *user_data;
	bool is_line_present;
};

static inline const struct ctr_x10_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x10_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x10_set_handler_(const struct device *dev, ctr_x10_user_cb user_cb, void *user_data)
{
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_x10_get_line_voltage_(const struct device *dev, int *line_voltage_mv)
{
	int ret;

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	ret = adc_read(get_config(dev)->adc_dev, &get_data(dev)->line_adc_sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	int32_t valp = get_data(dev)->adc_buf[0] * (R7_KOHM + R8_KOHM) / R8_KOHM;
	ret = adc_raw_to_millivolts(adc_ref_internal(get_config(dev)->adc_dev),
				    get_config(dev)->line_adc_channel_cfg.gain,
				    get_data(dev)->line_adc_sequence.resolution, &valp);
	if (ret) {
		LOG_ERR("Call `adc_raw_to_millivolts` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	*line_voltage_mv = valp;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_x10_get_battery_voltage_(const struct device *dev, int *battery_voltage_mv)
{
	int ret;

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	ret = adc_read(get_config(dev)->adc_dev, &get_data(dev)->battery_adc_sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	int32_t valp = get_data(dev)->adc_buf[0] * (R33_MOHM + R35_MOHM) / R35_MOHM;
	ret = adc_raw_to_millivolts(adc_ref_internal(get_config(dev)->adc_dev),
				    get_config(dev)->battery_adc_channel_cfg.gain,
				    get_data(dev)->battery_adc_sequence.resolution, &valp);
	if (ret) {
		LOG_ERR("Call `adc_raw_to_millivolts` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	*battery_voltage_mv = valp;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_x10_get_line_present_(const struct device *dev, bool *is_line_present)
{
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);
	*is_line_present = get_data(dev)->is_line_present;
	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void timer_handler(struct k_timer *timer)
{
	const struct device *dev = CONTAINER_OF(timer, struct ctr_x10_data, timer)->dev;

	k_work_submit(&get_data(dev)->work);
}

static void work_handler(struct k_work *work)
{
	int ret;

	const struct device *dev = CONTAINER_OF(work, struct ctr_x10_data, work)->dev;

	int voltage_mv;
	ret = ctr_x10_get_line_voltage(dev, &voltage_mv);
	if (ret) {
		LOG_ERR("Call `ctr_x10_get_line_voltage` failed: %d", ret);
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->is_line_present) {
		if (voltage_mv < get_config(dev)->line_threshold_min) {
			get_data(dev)->is_line_present = false;

			if (get_data(dev)->user_cb) {
				get_data(dev)->user_cb(dev, CTR_X10_EVENT_LINE_DISCONNECTED,
						       get_data(dev)->user_data);
			}
		}
	} else {
		if (voltage_mv > get_config(dev)->line_threshold_max) {
			get_data(dev)->is_line_present = true;

			if (get_data(dev)->user_cb) {
				get_data(dev)->user_cb(dev, CTR_X10_EVENT_LINE_CONNECTED,
						       get_data(dev)->user_data);
			}
		}
	}

	k_mutex_unlock(&get_data(dev)->lock);
}

static int ctr_x10_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	k_mutex_init(&get_data(dev)->lock);

	get_data(dev)->line_adc_sequence.buffer = get_data(dev)->adc_buf;
	get_data(dev)->line_adc_sequence.buffer_size = sizeof(get_data(dev)->adc_buf);
	get_data(dev)->battery_adc_sequence.buffer = get_data(dev)->adc_buf;
	get_data(dev)->battery_adc_sequence.buffer_size = sizeof(get_data(dev)->adc_buf);

	if (!device_is_ready(get_config(dev)->adc_dev)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}

	ret = adc_channel_setup(get_config(dev)->adc_dev, &get_config(dev)->line_adc_channel_cfg);
	if (ret) {
		LOG_ERR("Call `adc_channel_setup` failed: %d", ret);
		return ret;
	}

	k_timer_start(&get_data(dev)->timer, K_MSEC(get_config(dev)->line_measurement_interval),
		      K_MSEC(get_config(dev)->line_measurement_interval));

	int line_voltage_mv;
	ret = ctr_x10_get_line_voltage(dev, &line_voltage_mv);
	if (ret) {
		LOG_ERR("Call `ctr_x10_get_line_voltage` failed: %d", ret);
		return ret;
	}

	get_data(dev)->is_line_present = line_voltage_mv > get_config(dev)->line_threshold_min;

	return 0;
}

static const struct ctr_x10_driver_api ctr_x10_driver_api = {
	.set_handler = ctr_x10_set_handler_,
	.get_line_voltage = ctr_x10_get_line_voltage_,
	.get_line_present = ctr_x10_get_line_present_,
	.get_battery_voltage = ctr_x10_get_battery_voltage_,
};

#define CTR_X10_INIT(n)                                                                            \
	static const struct ctr_x10_config inst_##n##_config = {                                   \
		.adc_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x10_tla2024)),                           \
		.line_adc_channel_cfg =                                                            \
			{                                                                          \
				.gain = ADC_GAIN_1,                                                \
				.reference = ADC_REF_INTERNAL,                                     \
				.acquisition_time = ADC_ACQ_TIME_DEFAULT,                          \
				.channel_id = BIT(0),                                              \
				.differential = 1,                                                 \
			},                                                                         \
		.battery_adc_channel_cfg =                                                         \
			{                                                                          \
				.gain = ADC_GAIN_1,                                                \
				.reference = ADC_REF_INTERNAL,                                     \
				.acquisition_time = ADC_ACQ_TIME_DEFAULT,                          \
				.channel_id = BIT(1),                                              \
				.differential = 1,                                                 \
			},                                                                         \
		.line_measurement_interval = DT_INST_PROP(n, line_measurement_interval),           \
		.line_threshold_min = DT_INST_PROP(n, line_threshold_min),                         \
		.line_threshold_max = DT_INST_PROP(n, line_threshold_max),                         \
	};                                                                                         \
	static struct ctr_x10_data inst_##n##_data = {                                             \
		.timer = Z_TIMER_INITIALIZER(inst_##n##_data.timer, timer_handler, NULL),          \
		.work = Z_WORK_INITIALIZER(work_handler),                                          \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.line_adc_sequence = {.channels = BIT(0), .resolution = 12},                       \
		.battery_adc_sequence = {.channels = BIT(1), .resolution = 12},                    \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_x10_init, NULL, &inst_##n##_data, &inst_##n##_config,         \
			      POST_KERNEL, CONFIG_CTR_X10_INIT_PRIORITY, &ctr_x10_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X10_INIT)
