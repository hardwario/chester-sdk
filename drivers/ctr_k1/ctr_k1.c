/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_k1.h>

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

#define DT_DRV_COMPAT hardwario_ctr_k1

LOG_MODULE_REGISTER(ctr_k1, CONFIG_CTR_K1_LOG_LEVEL);

#define STEP_UP_START_DELAY K_MSEC(100)
#define STEP_UP_STOP_DELAY  K_MSEC(10)
#define MAX_CHANNEL_COUNT   4
#define MAX_SAMPLE_COUNT    500
#define SAMPLE_INTERVAL_US  1000

struct ctr_k1_config {
	const struct gpio_dt_spec on1_spec;
	const struct gpio_dt_spec on2_spec;
	const struct gpio_dt_spec on3_spec;
	const struct gpio_dt_spec on4_spec;
	const struct gpio_dt_spec en_spec;
	const struct gpio_dt_spec nc1_spec;
	const struct gpio_dt_spec nc2_spec;
	const struct gpio_dt_spec nc3_spec;
};

struct ctr_k1_data {
	const struct device *dev;
	bool is_on1_enabled;
	bool is_on2_enabled;
	bool is_on3_enabled;
	bool is_on4_enabled;
};

static const nrfx_timer_t m_timer = NRFX_TIMER_INSTANCE(4);
static nrf_saadc_value_t m_samples[MAX_CHANNEL_COUNT * MAX_SAMPLE_COUNT];
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

static inline const struct ctr_k1_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_k1_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int get_spec(const struct device *dev, enum ctr_k1_channel channel,
		    const struct gpio_dt_spec **spec)
{
	switch (channel) {
	case CTR_K1_CHANNEL_1_SINGLE_ENDED:
	case CTR_K1_CHANNEL_1_DIFFERENTIAL:
		*spec = &get_config(dev)->on1_spec;
		break;
	case CTR_K1_CHANNEL_2_SINGLE_ENDED:
	case CTR_K1_CHANNEL_2_DIFFERENTIAL:
		*spec = &get_config(dev)->on2_spec;
		break;
	case CTR_K1_CHANNEL_3_SINGLE_ENDED:
	case CTR_K1_CHANNEL_3_DIFFERENTIAL:
		*spec = &get_config(dev)->on3_spec;
		break;
	case CTR_K1_CHANNEL_4_SINGLE_ENDED:
	case CTR_K1_CHANNEL_4_DIFFERENTIAL:
		*spec = &get_config(dev)->on4_spec;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	return 0;
}

static int ctr_k1_set_power_(const struct device *dev, enum ctr_k1_channel channel, bool is_enabled)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	if (is_enabled) {
		if (!get_data(dev)->is_on1_enabled && !get_data(dev)->is_on2_enabled &&
		    !get_data(dev)->is_on3_enabled && !get_data(dev)->is_on4_enabled) {
			if (!device_is_ready(get_config(dev)->en_spec.port)) {
				LOG_ERR("Device not ready");
				k_mutex_unlock(&m_lock);
				return -ENODEV;
			}

			ret = gpio_pin_set_dt(&get_config(dev)->en_spec, 1);
			if (ret) {
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
				k_mutex_unlock(&m_lock);
				return ret;
			}

			k_sleep(STEP_UP_START_DELAY);
		}
	}

	const struct gpio_dt_spec *spec;
	ret = get_spec(dev, channel, &spec);
	if (ret) {
		LOG_ERR("Call `get_on_spec` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	if (!device_is_ready(spec->port)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&m_lock);
		return -ENODEV;
	}

	ret = gpio_pin_set_dt(spec, is_enabled ? 1 : 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	switch (channel) {
	case CTR_K1_CHANNEL_1_SINGLE_ENDED:
	case CTR_K1_CHANNEL_1_DIFFERENTIAL:
		get_data(dev)->is_on1_enabled = is_enabled;
		break;
	case CTR_K1_CHANNEL_2_SINGLE_ENDED:
	case CTR_K1_CHANNEL_2_DIFFERENTIAL:
		get_data(dev)->is_on2_enabled = is_enabled;
		break;
	case CTR_K1_CHANNEL_3_SINGLE_ENDED:
	case CTR_K1_CHANNEL_3_DIFFERENTIAL:
		get_data(dev)->is_on3_enabled = is_enabled;
		break;
	case CTR_K1_CHANNEL_4_SINGLE_ENDED:
	case CTR_K1_CHANNEL_4_DIFFERENTIAL:
		get_data(dev)->is_on4_enabled = is_enabled;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		k_mutex_unlock(&m_lock);
		return -EINVAL;
	}

	if (!is_enabled) {
		if (!get_data(dev)->is_on1_enabled && !get_data(dev)->is_on2_enabled &&
		    !get_data(dev)->is_on3_enabled && !get_data(dev)->is_on4_enabled) {
			if (!device_is_ready(get_config(dev)->en_spec.port)) {
				LOG_ERR("Device not ready");
				k_mutex_unlock(&m_lock);
				return -ENODEV;
			}

			ret = gpio_pin_set_dt(&get_config(dev)->en_spec, 0);
			if (ret) {
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
				k_mutex_unlock(&m_lock);
				return ret;
			}

			k_sleep(STEP_UP_STOP_DELAY);
		}
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

static int ctr_k1_measure_(const struct device *dev, const enum ctr_k1_channel channels[],
			   size_t channels_count, const struct ctr_k1_calibration calibrations[],
			   struct ctr_k1_result results[])
{
	int ret;

	if (!channels_count || channels_count > MAX_CHANNEL_COUNT) {
		return -EINVAL;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	nrfx_saadc_channel_t saadc_channels[MAX_CHANNEL_COUNT] = {0};
	float (*convert[MAX_CHANNEL_COUNT])(nrf_saadc_value_t) = {0};

	for (size_t i = 0; i < channels_count; i++) {
		results[i].avg = 0.f;
		results[i].rms = 0.f;

		saadc_channels[i].channel_index = i;

		switch (channels[i]) {
		case CTR_K1_CHANNEL_1_SINGLE_ENDED:
			saadc_channels[i].channel_config = m_channel_config_single_ended;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN1;
			saadc_channels[i].pin_n = NRF_SAADC_INPUT_DISABLED;
			convert[i] = convert_single_ended_to_millivolts;
			break;
		case CTR_K1_CHANNEL_2_SINGLE_ENDED:
			saadc_channels[i].channel_config = m_channel_config_single_ended;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN0;
			saadc_channels[i].pin_n = NRF_SAADC_INPUT_DISABLED;
			convert[i] = convert_single_ended_to_millivolts;
			break;
		case CTR_K1_CHANNEL_3_SINGLE_ENDED:
			saadc_channels[i].channel_config = m_channel_config_single_ended;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN4;
			saadc_channels[i].pin_n = NRF_SAADC_INPUT_DISABLED;
			convert[i] = convert_single_ended_to_millivolts;
			break;
		case CTR_K1_CHANNEL_4_SINGLE_ENDED:
			saadc_channels[i].channel_config = m_channel_config_single_ended;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN2;
			saadc_channels[i].pin_n = NRF_SAADC_INPUT_DISABLED;
			convert[i] = convert_single_ended_to_millivolts;
			break;
		case CTR_K1_CHANNEL_1_DIFFERENTIAL:
			saadc_channels[i].channel_config = m_channel_config_differential;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN5;
			saadc_channels[i].pin_n = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN1;
			convert[i] = convert_differential_to_millivolts;
			break;
		case CTR_K1_CHANNEL_2_DIFFERENTIAL:
			saadc_channels[i].channel_config = m_channel_config_differential;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN7;
			saadc_channels[i].pin_n = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN0;
			convert[i] = convert_differential_to_millivolts;
			break;
		case CTR_K1_CHANNEL_3_DIFFERENTIAL:
			saadc_channels[i].channel_config = m_channel_config_differential;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN6;
			saadc_channels[i].pin_n = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN4;
			convert[i] = convert_differential_to_millivolts;
			break;
		case CTR_K1_CHANNEL_4_DIFFERENTIAL:
			saadc_channels[i].channel_config = m_channel_config_differential;
			saadc_channels[i].pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN3;
			saadc_channels[i].pin_n = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN2;
			convert[i] = convert_differential_to_millivolts;
			break;
		default:
			LOG_ERR("Unknown channel: %d", channels[i]);
			return -EINVAL;
		}
	}

	k_mutex_lock(&m_lock, K_FOREVER);

	ret = measure(saadc_channels, channels_count);
	if (ret) {
		LOG_ERR("Call `measure` failed: %d", ret);
		k_mutex_unlock(&m_lock);
		return ret;
	}

	float raw_avg[MAX_CHANNEL_COUNT] = {0};
	float raw_rms[MAX_CHANNEL_COUNT] = {0};

	for (size_t i = 0; i < channels_count * MAX_SAMPLE_COUNT; i += channels_count) {
		for (size_t j = 0; j < channels_count; j++) {
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

	for (size_t i = 0; i < channels_count; i++) {
		unsigned ch = CTR_K1_CHANNEL_IDX(channels[i]) + 1;

		raw_avg[i] = raw_avg[i] / MAX_SAMPLE_COUNT;
		raw_rms[i] = sqrtf(raw_rms[i] / MAX_SAMPLE_COUNT);

		LOG_DBG("Channel %u: AVG (raw): %+.1f", ch, raw_avg[i]);
		LOG_DBG("Channel %u: RMS (raw): %+.1f", ch, raw_rms[i]);

		results[i].avg = results[i].avg / MAX_SAMPLE_COUNT;
		results[i].rms = sqrtf(results[i].rms / MAX_SAMPLE_COUNT);

		LOG_DBG("Channel %u: AVG (cal): %+.1f", ch, results[i].avg);
		LOG_DBG("Channel %u: RMS (cal): %+.1f", ch, results[i].rms);
	}

	return 0;
}

static int ctr_k1_init(const struct device *dev)
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

#define CHECK_READY(name)                                                                          \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->name##_spec.port)) {                         \
			LOG_ERR("Device not ready");                                               \
			return -ENODEV;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY(on1);
	CHECK_READY(on2);
	CHECK_READY(on3);
	CHECK_READY(on4);
	CHECK_READY(en);
	CHECK_READY(nc1);
	CHECK_READY(nc2);
	CHECK_READY(nc3);

#undef CHECK_READY

#define SETUP_PIN(name)                                                                            \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->name##_spec, GPIO_OUTPUT_INACTIVE);  \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	SETUP_PIN(on1);
	SETUP_PIN(on2);
	SETUP_PIN(on3);
	SETUP_PIN(on4);
	SETUP_PIN(en);
	SETUP_PIN(nc1);
	SETUP_PIN(nc2);
	SETUP_PIN(nc3);

#undef SETUP_PIN

	return 0;
}

static const struct ctr_k1_driver_api ctr_k1_driver_api = {
	.set_power = ctr_k1_set_power_,
	.measure = ctr_k1_measure_,
};

#define CTR_K1_INIT(n)                                                                             \
	static const struct ctr_k1_config inst_##n##_config = {                                    \
		.on1_spec = GPIO_DT_SPEC_INST_GET(n, on1_gpios),                                   \
		.on2_spec = GPIO_DT_SPEC_INST_GET(n, on2_gpios),                                   \
		.on3_spec = GPIO_DT_SPEC_INST_GET(n, on3_gpios),                                   \
		.on4_spec = GPIO_DT_SPEC_INST_GET(n, on4_gpios),                                   \
		.en_spec = GPIO_DT_SPEC_INST_GET(n, en_gpios),                                     \
		.nc1_spec = GPIO_DT_SPEC_INST_GET(n, nc1_gpios),                                   \
		.nc2_spec = GPIO_DT_SPEC_INST_GET(n, nc2_gpios),                                   \
		.nc3_spec = GPIO_DT_SPEC_INST_GET(n, nc3_gpios),                                   \
	};                                                                                         \
	static struct ctr_k1_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_k1_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_CTR_K1_INIT_PRIORITY, &ctr_k1_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_K1_INIT)
