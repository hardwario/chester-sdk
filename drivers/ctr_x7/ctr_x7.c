/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x7.h>

/* Nordic includes */
#include <hal/nrf_saadc.h>
#include <nrf52840.h>
#include <nrfx.h>
#include <nrfx_ppi.h>
#include <nrfx_saadc.h>
#include <nrfx_timer.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_x7

LOG_MODULE_REGISTER(ctr_x7, CONFIG_CTR_X7_LOG_LEVEL);

#define STEP_UP_START_DELAY K_MSEC(30)
#define STEP_UP_STOP_DELAY  K_MSEC(30)
#define MAX_SAMPLE_COUNT    500
#define SAMPLE_INTERVAL_US  1000

struct ctr_x7_config {
	const struct gpio_dt_spec en_spec;
};

struct ctr_x7_data {
	const struct device *dev;
};

static const nrfx_timer_t m_timer = NRFX_TIMER_INSTANCE(4);
static nrf_saadc_value_t m_samples[2 * MAX_SAMPLE_COUNT];
static K_MUTEX_DEFINE(m_lock);
static K_SEM_DEFINE(m_adc_sem, 0, 1);

static const nrf_saadc_channel_config_t m_channel_config_single_ended = {
	.resistor_p = NRF_SAADC_RESISTOR_DISABLED,
	.resistor_n = NRF_SAADC_RESISTOR_DISABLED,
	.gain = NRF_SAADC_GAIN1_6,
	.reference = NRF_SAADC_REFERENCE_INTERNAL,
	.acq_time = NRF_SAADC_ACQTIME_10US,
	.mode = NRF_SAADC_MODE_SINGLE_ENDED,
	.burst = NRF_SAADC_BURST_ENABLED,
};

static const nrf_saadc_channel_config_t m_channel_config_differential = {
	.resistor_p = NRF_SAADC_RESISTOR_DISABLED,
	.resistor_n = NRF_SAADC_RESISTOR_DISABLED,
	.gain = NRF_SAADC_GAIN1_6,
	.reference = NRF_SAADC_REFERENCE_INTERNAL,
	.acq_time = NRF_SAADC_ACQTIME_10US,
	.mode = NRF_SAADC_MODE_DIFFERENTIAL,
	.burst = NRF_SAADC_BURST_ENABLED,
};

static inline const struct ctr_x7_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x7_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x7_set_power_(const struct device *dev, bool is_enabled)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	if (!device_is_ready(get_config(dev)->en_spec.port)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_lock);
		return -ENODEV;
	}

	if (is_enabled) {
		ret = gpio_pin_set_dt(&get_config(dev)->en_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		k_sleep(STEP_UP_START_DELAY);

	} else {
		ret = gpio_pin_set_dt(&get_config(dev)->en_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
			k_mutex_unlock(&m_lock);
			return ret;
		}

		k_sleep(STEP_UP_STOP_DELAY);
	}

	k_mutex_unlock(&m_lock);

	return 0;
}

static void timer_event_handler(nrf_timer_event_t event_type, void *p_context)
{
}

static int setup_timer(void)
{
	nrfx_err_t ret_nrfx;

	IRQ_CONNECT(TIMER4_IRQn, 0, nrfx_timer_4_irq_handler, NULL, 0);
	irq_enable(TIMER4_IRQn);

	uint32_t base_frequency = NRF_TIMER_BASE_FREQUENCY_GET(m_timer.p_reg);
	nrfx_timer_config_t timer_config = NRFX_TIMER_DEFAULT_CONFIG(base_frequency);

	ret_nrfx = nrfx_timer_init(&m_timer, &timer_config, timer_event_handler);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_timer_init` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	nrfx_timer_extended_compare(&m_timer, NRF_TIMER_CC_CHANNEL0,
				    nrfx_timer_us_to_ticks(&m_timer, SAMPLE_INTERVAL_US),
				    NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, false);

	return 0;
}

static int setup_saadc(void)
{
	IRQ_CONNECT(SAADC_IRQn, 0, nrfx_saadc_irq_handler, NULL, 0);
	irq_enable(SAADC_IRQn);

	return 0;
}

static int setup_ppi(void)
{
	nrfx_err_t ret_nrfx;

	nrf_ppi_channel_t ppi_channel;

	ret_nrfx = nrfx_ppi_channel_alloc(&ppi_channel);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_ppi_channel_alloc` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_ppi_channel_assign(
		ppi_channel, nrfx_timer_event_address_get(&m_timer, NRF_TIMER_EVENT_COMPARE0),
		nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE));
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_ppi_channel_assign` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_ppi_channel_enable(ppi_channel);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_ppi_channel_enable` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	return 0;
}

static void saadc_event_handler(nrfx_saadc_evt_t const *p_event)
{
	switch (p_event->type) {
	case NRFX_SAADC_EVT_DONE:
		LOG_DBG("Event `NRFX_SAADC_EVT_DONE`");
		break;
	case NRFX_SAADC_EVT_CALIBRATEDONE:
		LOG_DBG("Event `NRFX_SAADC_EVT_CALIBRATEDONE`");
		k_sem_give(&m_adc_sem);
		break;
	case NRFX_SAADC_EVT_READY:
		LOG_DBG("Event `NRFX_SAADC_EVT_READY`");
		nrfx_timer_enable(&m_timer);
		break;
	case NRFX_SAADC_EVT_FINISHED:
		LOG_DBG("Event `NRFX_SAADC_EVT_FINISHED`");
		nrfx_timer_disable(&m_timer);
		k_sem_give(&m_adc_sem);
		break;
	default:
		break;
	}
}

static int measure(const nrfx_saadc_channel_t saadc_channels[], size_t saadc_channels_count)
{
	int ret;
	nrfx_err_t ret_nrfx;

	ret_nrfx = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_init` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

#if 0
	ret_nrfx = nrfx_saadc_offset_calibrate(saadc_event_handler);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_calibrate_offset` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret = k_sem_take(&m_adc_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		return ret;
	}
#endif

	ret_nrfx = nrfx_saadc_channels_config(saadc_channels, saadc_channels_count);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_channels_config` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	nrfx_saadc_adv_config_t config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
#if 0
	config.oversampling = NRF_SAADC_OVERSAMPLE_16X;
	config.burst = NRF_SAADC_BURST_ENABLED;
#endif

	ret_nrfx = nrfx_saadc_advanced_mode_set(BIT_MASK(saadc_channels_count),
						NRF_SAADC_RESOLUTION_12BIT, &config,
						saadc_event_handler);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_advanced_mode_set` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_saadc_buffer_set(m_samples, saadc_channels_count * MAX_SAMPLE_COUNT);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_buffer_set` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_saadc_mode_trigger();
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_mode_trigger` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret = k_sem_take(&m_adc_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		return ret;
	}

	nrfx_saadc_uninit();

	return 0;
}

static inline float convert_single_ended_to_millivolts(nrf_saadc_value_t value)
{
	return (float)value * 6 * 600 / 4096;
}

static inline float convert_differential_to_millivolts(nrf_saadc_value_t value)
{
	return (float)value * 6 * 600 / 2048;
}

static int ctr_x7_measure_(const struct device *dev,
			   const struct ctr_x7_calibration calibrations[2],
			   struct ctr_x7_result results[2])
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	results[0].avg = 0.f;
	results[0].rms = 0.f;

	results[1].avg = 0.f;
	results[1].rms = 0.f;

	nrfx_saadc_channel_t saadc_channels[2] = {0};

	saadc_channels[0].channel_index = 0;
	saadc_channels[0].channel_config = m_channel_config_differential;
	saadc_channels[0].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN1;
	saadc_channels[0].pin_n = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN5;

	saadc_channels[1].channel_index = 1;
	saadc_channels[1].channel_config = m_channel_config_single_ended;
	saadc_channels[1].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN0;
	saadc_channels[1].pin_n = NRF_SAADC_INPUT_DISABLED;

	float (*convert[2])(nrf_saadc_value_t) = {0};

	convert[0] = convert_differential_to_millivolts;
	convert[1] = convert_single_ended_to_millivolts;

	k_mutex_lock(&m_lock, K_FOREVER);

	ret = measure(saadc_channels, 2);
	if (ret) {
		LOG_ERR("Call `measure` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	float raw_avg[2] = {0};
	float raw_rms[2] = {0};

	for (size_t i = 0; i < 2 * MAX_SAMPLE_COUNT; i += 2) {
		for (size_t j = 0; j < 2; j++) {
			float x = convert[j](m_samples[i + j]);

			raw_avg[j] += x;
			raw_rms[j] += powf(x, 2);

			if (!isnan(calibrations[j].x0) && !isnan(calibrations[j].y0) &&
			    !isnan(calibrations[j].x1) && !isnan(calibrations[j].y1)) {
				float x0 = calibrations[j].x0;
				float y0 = calibrations[j].y0;
				float x1 = calibrations[j].x1;
				float y1 = calibrations[j].y1;

				x = (y0 * (x1 - x) + y1 * (x - x0)) / (x1 - x0);
			}

			results[j].avg += x;
			results[j].rms += powf(x, 2);
		}
	}

	k_mutex_unlock(&m_lock);

	for (size_t i = 0; i < 2; i++) {
		raw_avg[i] = raw_avg[i] / MAX_SAMPLE_COUNT;
		raw_rms[i] = sqrtf(raw_rms[i] / MAX_SAMPLE_COUNT);

		LOG_DBG("Channel %u: AVG (raw): %+.1f", i + 1, raw_avg[i]);
		LOG_DBG("Channel %u: RMS (raw): %+.1f", i + 1, raw_rms[i]);

		results[i].avg = results[i].avg / MAX_SAMPLE_COUNT;
		results[i].rms = sqrtf(results[i].rms / MAX_SAMPLE_COUNT);

		LOG_DBG("Channel %u: AVG (cal): %+.1f", i + 1, results[i].avg);
		LOG_DBG("Channel %u: RMS (cal): %+.1f", i + 1, results[i].rms);
	}

	return 0;
}

static int ctr_x7_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	ret = setup_timer();
	if (ret) {
		LOG_ERR("Call `setup_timer` failed: %d", ret);
		return ret;
	}

	ret = setup_saadc();
	if (ret) {
		LOG_ERR("Call `setup_saadc` failed: %d", ret);
		return ret;
	}

	ret = setup_ppi();
	if (ret) {
		LOG_ERR("Call `setup_ppi` failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->en_spec.port)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->en_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_x7_driver_api ctr_x7_driver_api = {
	.set_power = ctr_x7_set_power_,
	.measure = ctr_x7_measure_,
};

#define CTR_X7_INIT(n)                                                                             \
	static const struct ctr_x7_config inst_##n##_config = {                                    \
		.en_spec = GPIO_DT_SPEC_INST_GET(n, en_gpios),                                     \
	};                                                                                         \
	static struct ctr_x7_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_x7_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_X7_INIT_PRIORITY, &ctr_x7_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X7_INIT)
