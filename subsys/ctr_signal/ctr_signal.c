/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include <chester/ctr_signal.h>

/* Nordic includes */
#include <hal/nrf_saadc.h>
#include <nrf52840.h>
#include <nrfx.h>
#include <nrfx_ppi.h>
#include <nrfx_saadc.h>
#include <nrfx_timer.h>

/* Zephyr includes */
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <math.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_signal, CONFIG_CTR_SIGNAL_LOG_LEVEL);

#define SAMPLE_INTERVAL_US           1000
#define SAMPLE_COUNT                 500
#define SAMPLE_TO_MILLIVOLTS(sample) (sample * 6 * 600 / 2048)

static const nrfx_timer_t m_timer = NRFX_TIMER_INSTANCE(4);
static nrf_saadc_value_t m_samples[SAMPLE_COUNT];
static K_SEM_DEFINE(m_sem, 0, 1);

static void timer_event_handler(nrf_timer_event_t event_type, void *p_context)
{
}

static void saadc_event_handler(nrfx_saadc_evt_t const *p_event)
{
	switch (p_event->type) {
	case NRFX_SAADC_EVT_DONE:
		LOG_DBG("Event `NRFX_SAADC_EVT_DONE`");
		k_sem_give(&m_sem);
		break;

	case NRFX_SAADC_EVT_CALIBRATEDONE:
		LOG_DBG("Event `NRFX_SAADC_EVT_CALIBRATEDONE`");
		k_sem_give(&m_sem);
		break;

	case NRFX_SAADC_EVT_READY:
		LOG_DBG("Event `NRFX_SAADC_EVT_READY`");
		nrfx_timer_enable(&m_timer);
		break;

	case NRFX_SAADC_EVT_FINISHED:
		LOG_DBG("Event `NRFX_SAADC_EVT_FINISHED`");
		nrfx_timer_disable(&m_timer);
		break;

	default:
		break;
	}
}

static int setup_timer(void)
{
	nrfx_err_t ret_nrfx;

	IRQ_CONNECT(TIMER1_IRQn, 0, nrfx_timer_1_irq_handler, NULL, 0);
	irq_enable(TIMER1_IRQn);

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

static int measure(void)
{
	int ret;
	nrfx_err_t ret_nrfx;

	ret_nrfx = nrfx_saadc_init(NRFX_SAADC_DEFAULT_CONFIG_IRQ_PRIORITY);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_init` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_saadc_offset_calibrate(saadc_event_handler);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_calibrate_offset` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret = k_sem_take(&m_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		return ret;
	}

	static const nrfx_saadc_channel_t channels[] = {
		{
			.channel_config =
				{
					.resistor_p = NRF_SAADC_RESISTOR_DISABLED,
					.resistor_n = NRF_SAADC_RESISTOR_DISABLED,
					.gain = NRF_SAADC_GAIN1_6,
					.reference = NRF_SAADC_REFERENCE_INTERNAL,
					.acq_time = NRF_SAADC_ACQTIME_40US,
					.mode = NRF_SAADC_MODE_DIFFERENTIAL,
					.burst = NRF_SAADC_BURST_ENABLED,
				},
			.pin_p = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN1,
			.pin_n = (nrf_saadc_input_t)NRF_SAADC_INPUT_AIN5,
			.channel_index = 0,
		},
	};

	ret_nrfx = nrfx_saadc_channels_config(channels, ARRAY_SIZE(channels));
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_channels_config` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	nrfx_saadc_adv_config_t config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
	config.oversampling = NRF_SAADC_OVERSAMPLE_8X;
	config.burst = NRF_SAADC_BURST_ENABLED;

	ret_nrfx = nrfx_saadc_advanced_mode_set(BIT(0), NRF_SAADC_RESOLUTION_12BIT, &config,
						saadc_event_handler);
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_advanced_mode_set` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_saadc_buffer_set(m_samples, ARRAY_SIZE(m_samples));
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_buffer_set` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret_nrfx = nrfx_saadc_mode_trigger();
	if (ret_nrfx != NRFX_SUCCESS) {
		LOG_ERR("Call `nrfx_saadc_mode_trigger` failed: 0x%08x", ret_nrfx);
		return -EIO;
	}

	ret = k_sem_take(&m_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		return ret;
	}

	nrfx_saadc_uninit();

	return 0;
}

int ctr_signal_measure(double *avg, double *rms)
{
	int ret;

	*avg = 0;
	*rms = 0;

	ret = measure();
	if (ret) {
		LOG_ERR("Call `measure` failed: %d", ret);
		return ret;
	}

	for (size_t i = 0; i < ARRAY_SIZE(m_samples); i++) {
		int millivolts = SAMPLE_TO_MILLIVOLTS(m_samples[i]);

		*avg += millivolts;
		*rms += millivolts * millivolts;
	}

	*avg /= ARRAY_SIZE(m_samples);
	*rms = sqrt(*rms / ARRAY_SIZE(m_samples));

	LOG_DBG("AVG: %.1f RMS: %.1f", *avg, *rms);

	return 0;
}

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	ret = setup_timer();
	if (ret) {
		LOG_ERR("Call `setup_timer` failed: %d", ret);
		k_oops();
	}

	ret = setup_saadc();
	if (ret) {
		LOG_ERR("Call `setup_saadc` failed: %d", ret);
		k_oops();
	}

	ret = setup_ppi();
	if (ret) {
		LOG_ERR("Call `setup_ppi` failed: %d", ret);
		k_oops();
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
