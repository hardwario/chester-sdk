/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_s3.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Nordic includes */
#include <nrfx_pwm.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DT_DRV_COMPAT hardwario_ctr_s3

LOG_MODULE_REGISTER(ctr_s3, CONFIG_CTR_S3_LOG_LEVEL);

struct pyq1648_waveform {
	nrf_pwm_values_common_t values[25 + 1];
	int length;
};

struct ctr_s3_config {
	const struct gpio_dt_spec si_spec_0;
	const struct gpio_dt_spec si_spec_1;
	const struct gpio_dt_spec dl_spec_0;
	const struct gpio_dt_spec dl_spec_1;
};

struct ctr_s3_data {
	const struct device *dev;
	struct k_mutex lock;
	ctr_s3_user_cb user_cb;
	void *user_data;
	nrfx_pwm_t pwm_0;
	nrfx_pwm_t pwm_1;
	nrfx_pwm_config_t pwm_config_0;
	nrfx_pwm_config_t pwm_config_1;
	nrf_pwm_sequence_t pwm_sequence_0;
	nrf_pwm_sequence_t pwm_sequence_1;
	struct pyq1648_waveform wf_0;
	struct pyq1648_waveform wf_1;
	struct gpio_callback gpio_callback_0;
	struct gpio_callback gpio_callback_1;
	struct k_work clear_a_work_0;
	struct k_work clear_a_work_1;
	struct k_work_delayable clear_b_work_0;
	struct k_work_delayable clear_b_work_1;
	struct k_work_delayable clear_c_work_0;
	struct k_work_delayable clear_c_work_1;
};

static inline const struct ctr_s3_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_s3_data *get_data(const struct device *dev)
{
	return dev->data;
}

static void write_bit(struct pyq1648_waveform *wf, int value)
{
	wf->values[wf->length++] = 0x8000 | (value ? 95 : 5);
}

static void write_field(struct pyq1648_waveform *wf, uint8_t value, int count)
{
	for (int i = 0; i < count; i++) {
		write_bit(wf, value & (1 << (count - i - 1)) ? 1 : 0);
	}
}

static void gpio_handler_0(const struct device *port, struct gpio_callback *cb,
			   gpio_port_pins_t pins)
{
	int ret;

	const struct device *dev = CONTAINER_OF(cb, struct ctr_s3_data, gpio_callback_0)->dev;

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_0, GPIO_INT_DISABLE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}

	ret = k_work_submit(&get_data(dev)->clear_a_work_0);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit` failed: %d", ret);
	}
}

static void gpio_handler_1(const struct device *port, struct gpio_callback *cb,
			   gpio_port_pins_t pins)
{
	int ret;

	const struct device *dev = CONTAINER_OF(cb, struct ctr_s3_data, gpio_callback_1)->dev;

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_1, GPIO_INT_DISABLE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}

	ret = k_work_submit(&get_data(dev)->clear_a_work_1);
	if (ret < 0) {
		LOG_ERR("Call `k_work_submit` failed: %d", ret);
	}
}

static void clear_a_work_handler_0(struct k_work *work)
{
	int ret;

	const struct device *dev = CONTAINER_OF(work, struct ctr_s3_data, clear_a_work_0)->dev;

	ret = gpio_pin_configure_dt(&get_config(dev)->dl_spec_0, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
	}

	LOG_DBG("Motion detected");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->user_cb) {
		get_data(dev)->user_cb(dev, CTR_S3_EVENT_MOTION_L_DETECTED,
				       get_data(dev)->user_data);
	}

	k_mutex_unlock(&get_data(dev)->lock);

	ret = k_work_schedule(&get_data(dev)->clear_b_work_0, K_MSEC(10));
	if (ret < 0) {
		LOG_ERR("Call `k_work_schedule` failed: %d", ret);
	}
}

static void clear_a_work_handler_1(struct k_work *work)
{
	int ret;

	const struct device *dev = CONTAINER_OF(work, struct ctr_s3_data, clear_a_work_1)->dev;

	ret = gpio_pin_configure_dt(&get_config(dev)->dl_spec_1, GPIO_OUTPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
	}

	LOG_DBG("Motion detected");

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->user_cb) {
		get_data(dev)->user_cb(dev, CTR_S3_EVENT_MOTION_R_DETECTED,
				       get_data(dev)->user_data);
	}

	k_mutex_unlock(&get_data(dev)->lock);

	ret = k_work_schedule(&get_data(dev)->clear_b_work_1, K_MSEC(10));
	if (ret < 0) {
		LOG_ERR("Call `k_work_schedule` failed: %d", ret);
	}
}

static void clear_b_work_handler_0(struct k_work *work)
{
	int ret;

	const struct device *dev =
		CONTAINER_OF((struct k_work_delayable *)work, struct ctr_s3_data, clear_b_work_0)
			->dev;

	ret = gpio_pin_configure_dt(&get_config(dev)->dl_spec_0, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
	}

	ret = k_work_schedule(&get_data(dev)->clear_c_work_0, K_MSEC(10));
	if (ret < 0) {
		LOG_ERR("Call `k_work_schedule` failed: %d", ret);
	}
}

static void clear_b_work_handler_1(struct k_work *work)
{
	int ret;

	const struct device *dev =
		CONTAINER_OF((struct k_work_delayable *)work, struct ctr_s3_data, clear_b_work_1)
			->dev;

	ret = gpio_pin_configure_dt(&get_config(dev)->dl_spec_1, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
	}

	ret = k_work_schedule(&get_data(dev)->clear_c_work_1, K_MSEC(10));
	if (ret < 0) {
		LOG_ERR("Call `k_work_schedule` failed: %d", ret);
	}
}

static void clear_c_work_handler_0(struct k_work *work)
{
	int ret;

	const struct device *dev =
		CONTAINER_OF((struct k_work_delayable *)work, struct ctr_s3_data, clear_c_work_0)
			->dev;

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_0, GPIO_INT_LEVEL_HIGH);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}
}

static void clear_c_work_handler_1(struct k_work *work)
{
	int ret;

	const struct device *dev =
		CONTAINER_OF((struct k_work_delayable *)work, struct ctr_s3_data, clear_c_work_1)
			->dev;

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_1, GPIO_INT_LEVEL_HIGH);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}
}

static int ctr_s3_set_handler_(const struct device *dev, ctr_s3_user_cb user_cb, void *user_data)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_s3_configure_(const struct device *dev, enum ctr_s3_channel channel,
			     const struct ctr_s3_param *param)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (channel == CTR_S3_CHANNEL_L) {
		ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_0,
						      GPIO_INT_DISABLE);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->lock);
			return ret;
		}

		write_field(&get_data(dev)->wf_0, param->sensitivity, 8);
		write_field(&get_data(dev)->wf_0, param->blind_time, 4);
		write_field(&get_data(dev)->wf_0, param->pulse_counter, 2);
		write_field(&get_data(dev)->wf_0, param->window_time, 2);
		write_field(&get_data(dev)->wf_0, 2, 2);
		write_field(&get_data(dev)->wf_0, 0, 2);
		write_field(&get_data(dev)->wf_0, 2, 2);
		write_field(&get_data(dev)->wf_0, 1, 1);
		write_field(&get_data(dev)->wf_0, 0, 1);
		write_field(&get_data(dev)->wf_0, 0, 1);

		get_data(dev)->wf_0.values[get_data(dev)->wf_0.length++] = 0x8000;

		get_data(dev)->pwm_sequence_0.values.p_common = get_data(dev)->wf_0.values;
		get_data(dev)->pwm_sequence_0.length = get_data(dev)->wf_0.length;
		get_data(dev)->pwm_sequence_0.repeats = 0;
		get_data(dev)->pwm_sequence_0.end_delay = 0;

		k_sleep(K_MSEC(10));

		if (nrfx_pwm_simple_playback(&get_data(dev)->pwm_0, &get_data(dev)->pwm_sequence_0,
					     1, NRFX_PWM_FLAG_STOP)) {
			k_mutex_unlock(&get_data(dev)->lock);
			return -EIO;
		}

		k_sleep(K_MSEC(10));

		ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_0,
						      GPIO_INT_LEVEL_HIGH);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->lock);
			return ret;
		}

	} else if (channel == CTR_S3_CHANNEL_R) {
		ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_1,
						      GPIO_INT_DISABLE);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->lock);
			return ret;
		}

		write_field(&get_data(dev)->wf_1, param->sensitivity, 8);
		write_field(&get_data(dev)->wf_1, param->blind_time, 4);
		write_field(&get_data(dev)->wf_1, param->pulse_counter, 2);
		write_field(&get_data(dev)->wf_1, param->window_time, 2);
		write_field(&get_data(dev)->wf_1, 2, 2);
		write_field(&get_data(dev)->wf_1, 0, 2);
		write_field(&get_data(dev)->wf_1, 2, 2);
		write_field(&get_data(dev)->wf_1, 1, 1);
		write_field(&get_data(dev)->wf_1, 0, 1);
		write_field(&get_data(dev)->wf_1, 0, 1);

		get_data(dev)->wf_1.values[get_data(dev)->wf_1.length++] = 0x8000;

		get_data(dev)->pwm_sequence_1.values.p_common = get_data(dev)->wf_1.values;
		get_data(dev)->pwm_sequence_1.length = get_data(dev)->wf_1.length;
		get_data(dev)->pwm_sequence_1.repeats = 0;
		get_data(dev)->pwm_sequence_1.end_delay = 0;

		k_sleep(K_MSEC(10));

		if (nrfx_pwm_simple_playback(&get_data(dev)->pwm_1, &get_data(dev)->pwm_sequence_1,
					     1, NRFX_PWM_FLAG_STOP)) {
			k_mutex_unlock(&get_data(dev)->lock);
			return -EIO;
		}

		k_sleep(K_MSEC(10));

		ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->dl_spec_1,
						      GPIO_INT_LEVEL_HIGH);
		if (ret) {
			LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->lock);
			return ret;
		}

	} else {
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_s3_init(const struct device *dev)
{
	int ret;

	nrfx_err_t status;

	LOG_INF("System initialization");

	k_mutex_init(&get_data(dev)->lock);

	k_work_init(&get_data(dev)->clear_a_work_0, clear_a_work_handler_0);
	k_work_init(&get_data(dev)->clear_a_work_1, clear_a_work_handler_1);
	k_work_init_delayable(&get_data(dev)->clear_b_work_0, clear_b_work_handler_0);
	k_work_init_delayable(&get_data(dev)->clear_b_work_1, clear_b_work_handler_1);
	k_work_init_delayable(&get_data(dev)->clear_c_work_0, clear_c_work_handler_0);
	k_work_init_delayable(&get_data(dev)->clear_c_work_1, clear_c_work_handler_1);

	nrfx_pwm_config_t config =
		NRFX_PWM_DEFAULT_CONFIG(NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED,
					NRF_PWM_PIN_NOT_CONNECTED, NRF_PWM_PIN_NOT_CONNECTED);

	memcpy(&get_data(dev)->pwm_config_0, &config, sizeof(config));
	memcpy(&get_data(dev)->pwm_config_1, &config, sizeof(config));

	get_data(dev)->pwm_config_0.output_pins[0] = get_config(dev)->si_spec_0.pin;
	get_data(dev)->pwm_config_0.top_value = 100;

	get_data(dev)->pwm_config_1.output_pins[0] = get_config(dev)->si_spec_1.pin;
	get_data(dev)->pwm_config_1.top_value = 100;

	status = nrfx_pwm_init(&get_data(dev)->pwm_0, &get_data(dev)->pwm_config_0, NULL,
			       &get_data(dev)->pwm_0);

	if (status != NRFX_SUCCESS) {
		return -EIO;
	}

	status = nrfx_pwm_init(&get_data(dev)->pwm_1, &get_data(dev)->pwm_config_1, NULL,
			       &get_data(dev)->pwm_1);

	if (status != NRFX_SUCCESS) {
		return -EIO;
	}

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_PWM_INST_GET(0)), IRQ_PRIO_LOWEST,
			   NRFX_PWM_INST_HANDLER_GET(0), 0);

	IRQ_DIRECT_CONNECT(NRFX_IRQ_NUMBER_GET(NRF_PWM_INST_GET(1)), IRQ_PRIO_LOWEST,
			   NRFX_PWM_INST_HANDLER_GET(1), 0);

	ret = gpio_pin_configure_dt(&get_config(dev)->dl_spec_0, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->dl_spec_1, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	gpio_init_callback(&get_data(dev)->gpio_callback_0, gpio_handler_0,
			   BIT(get_config(dev)->dl_spec_0.pin));

	gpio_init_callback(&get_data(dev)->gpio_callback_1, gpio_handler_1,
			   BIT(get_config(dev)->dl_spec_1.pin));

	ret = gpio_add_callback_dt(&get_config(dev)->dl_spec_0, &get_data(dev)->gpio_callback_0);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_add_callback_dt(&get_config(dev)->dl_spec_1, &get_data(dev)->gpio_callback_1);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_s3_driver_api ctr_s3_driver_api = {
	.set_handler = ctr_s3_set_handler_,
	.configure = ctr_s3_configure_,
};

#define CTR_S3_INIT(n)                                                                             \
	static const struct ctr_s3_config inst_##n##_config = {                                    \
		.si_spec_0 = GPIO_DT_SPEC_INST_GET(n, left_si_gpios),                              \
		.si_spec_1 = GPIO_DT_SPEC_INST_GET(n, right_si_gpios),                             \
		.dl_spec_0 = GPIO_DT_SPEC_INST_GET(n, left_dl_gpios),                              \
		.dl_spec_1 = GPIO_DT_SPEC_INST_GET(n, right_dl_gpios),                             \
	};                                                                                         \
	static struct ctr_s3_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.pwm_0 = NRFX_PWM_INSTANCE(0),                                                     \
		.pwm_1 = NRFX_PWM_INSTANCE(1),                                                     \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_s3_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_S3_INIT_PRIORITY, &ctr_s3_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_S3_INIT)
