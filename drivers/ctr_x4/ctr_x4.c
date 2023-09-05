/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x4.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_x4

LOG_MODULE_REGISTER(ctr_x4, CONFIG_CTR_X4_LOG_LEVEL);

#define R1_KOHM 330
#define R2_KOHM 22

struct ctr_x4_config {
	const struct gpio_dt_spec pwr1_spec;
	const struct gpio_dt_spec pwr2_spec;
	const struct gpio_dt_spec pwr3_spec;
	const struct gpio_dt_spec pwr4_spec;
	const struct device *adc_dev;
	struct adc_channel_cfg adc_channel_cfg;
	int line_measurement_interval;
	int line_threshold_min;
	int line_threshold_max;
};

struct ctr_x4_data {
	struct k_mutex lock;
	struct k_timer timer;
	struct k_work work;
	const struct device *dev;
	struct adc_sequence adc_sequence;
	int16_t adc_buf[1];
	ctr_x4_user_cb user_cb;
	void *user_data;
	bool is_line_present;
};

static inline const struct ctr_x4_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x4_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x4_set_handler_(const struct device *dev, ctr_x4_user_cb user_cb, void *user_data)
{
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_x4_set_output_(const struct device *dev, enum ctr_x4_output output, bool is_on)
{
	int ret;

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

#define SET_OUTPUT(ch)                                                                             \
	do {                                                                                       \
		if (output == CTR_X4_OUTPUT_##ch) {                                                \
			if (!device_is_ready(get_config(dev)->pwr##ch##_spec.port)) {              \
				LOG_ERR("Device not ready");                                       \
				k_mutex_unlock(&get_data(dev)->lock);                              \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = gpio_pin_set_dt(&get_config(dev)->pwr##ch##_spec, is_on ? 1 : 0);    \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);                 \
				k_mutex_unlock(&get_data(dev)->lock);                              \
				return ret;                                                        \
			}                                                                          \
		}                                                                                  \
	} while (0)

	SET_OUTPUT(1);
	SET_OUTPUT(2);
	SET_OUTPUT(3);
	SET_OUTPUT(4);

#undef SET_OUTPUT

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_x4_get_line_voltage_(const struct device *dev, int *line_voltage_mv)
{
	int ret;

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	ret = adc_read(get_config(dev)->adc_dev, &get_data(dev)->adc_sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	int32_t valp = get_data(dev)->adc_buf[0] * (R1_KOHM + R2_KOHM) / R2_KOHM;
	ret = adc_raw_to_millivolts(adc_ref_internal(get_config(dev)->adc_dev),
				    get_config(dev)->adc_channel_cfg.gain,
				    get_data(dev)->adc_sequence.resolution, &valp);
	if (ret) {
		LOG_ERR("Call `adc_raw_to_millivolts` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	*line_voltage_mv = valp;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_x4_get_line_present_(const struct device *dev, bool *is_line_present)
{
	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);
	*is_line_present = get_data(dev)->is_line_present;
	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void timer_handler(struct k_timer *timer)
{
	const struct device *dev = CONTAINER_OF(timer, struct ctr_x4_data, timer)->dev;

	k_work_submit(&get_data(dev)->work);
}

static void work_handler(struct k_work *work)
{
	int ret;

	const struct device *dev = CONTAINER_OF(work, struct ctr_x4_data, work)->dev;

	int voltage_mv;
	ret = ctr_x4_get_line_voltage(dev, &voltage_mv);
	if (ret) {
		LOG_ERR("Call `ctr_x4_get_line_voltage` failed: %d", ret);
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->is_line_present) {
		if (voltage_mv < get_config(dev)->line_threshold_min) {
			get_data(dev)->is_line_present = false;

			if (get_data(dev)->user_cb) {
				get_data(dev)->user_cb(dev, CTR_X4_EVENT_LINE_DISCONNECTED,
						       get_data(dev)->user_data);
			}
		}
	} else {
		if (voltage_mv > get_config(dev)->line_threshold_max) {
			get_data(dev)->is_line_present = true;

			if (get_data(dev)->user_cb) {
				get_data(dev)->user_cb(dev, CTR_X4_EVENT_LINE_CONNECTED,
						       get_data(dev)->user_data);
			}
		}
	}

	k_mutex_unlock(&get_data(dev)->lock);
}

static int ctr_x4_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	k_mutex_init(&get_data(dev)->lock);

#define CHECK_READY(name)                                                                          \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->name##_spec.port)) {                         \
			LOG_ERR("Device not ready");                                               \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY(pwr1);
	CHECK_READY(pwr2);
	CHECK_READY(pwr3);
	CHECK_READY(pwr4);

#undef CHECK_READY

#define SETUP_OUTPUT(name)                                                                         \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->name##_spec, GPIO_OUTPUT_INACTIVE);  \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	SETUP_OUTPUT(pwr1);
	SETUP_OUTPUT(pwr2);
	SETUP_OUTPUT(pwr3);
	SETUP_OUTPUT(pwr4);

#undef SETUP_OUTPUT

	get_data(dev)->adc_sequence.buffer = get_data(dev)->adc_buf;
	get_data(dev)->adc_sequence.buffer_size = sizeof(get_data(dev)->adc_buf);

	if (!device_is_ready(get_config(dev)->adc_dev)) {
		LOG_ERR("ADC not ready");
		return -ENODEV;
	}

	ret = adc_channel_setup(get_config(dev)->adc_dev, &get_config(dev)->adc_channel_cfg);
	if (ret) {
		LOG_ERR("Call `adc_channel_setup` failed: %d", ret);
		return ret;
	}

	k_timer_start(&get_data(dev)->timer, K_MSEC(get_config(dev)->line_measurement_interval),
		      K_MSEC(get_config(dev)->line_measurement_interval));

	int line_voltage_mv;
	ret = ctr_x4_get_line_voltage(dev, &line_voltage_mv);
	if (ret) {
		LOG_ERR("Call `ctr_x4_get_line_voltage` failed: %d", ret);
		return ret;
	}

	get_data(dev)->is_line_present = line_voltage_mv > get_config(dev)->line_threshold_min;

	return 0;
}

static const struct ctr_x4_driver_api ctr_x4_driver_api = {
	.set_handler = ctr_x4_set_handler_,
	.set_output = ctr_x4_set_output_,
	.get_line_voltage = ctr_x4_get_line_voltage_,
	.get_line_present = ctr_x4_get_line_present_,
};

#define CTR_X4_INIT(n)                                                                             \
	static const struct ctr_x4_config inst_##n##_config = {                                    \
		.pwr1_spec = GPIO_DT_SPEC_INST_GET(n, pwr1_gpios),                                 \
		.pwr2_spec = GPIO_DT_SPEC_INST_GET(n, pwr2_gpios),                                 \
		.pwr3_spec = GPIO_DT_SPEC_INST_GET(n, pwr3_gpios),                                 \
		.pwr4_spec = GPIO_DT_SPEC_INST_GET(n, pwr4_gpios),                                 \
		.adc_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x4_tla2021)),                            \
		.adc_channel_cfg = {.gain = ADC_GAIN_1,                                            \
				    .reference = ADC_REF_INTERNAL,                                 \
				    .acquisition_time = ADC_ACQ_TIME_DEFAULT,                      \
				    .channel_id = BIT(0),                                          \
				    .differential = 1},                                            \
		.line_measurement_interval = DT_INST_PROP(n, line_measurement_interval),           \
		.line_threshold_min = DT_INST_PROP(n, line_threshold_min),                         \
		.line_threshold_max = DT_INST_PROP(n, line_threshold_max),                         \
	};                                                                                         \
	static struct ctr_x4_data inst_##n##_data = {                                              \
		.timer = Z_TIMER_INITIALIZER(inst_##n##_data.timer, timer_handler, NULL),          \
		.work = Z_WORK_INITIALIZER(work_handler),                                          \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.adc_sequence = {.channels = BIT(0), .resolution = 12},                            \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_x4_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_X4_INIT_PRIORITY, &ctr_x4_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X4_INIT)
