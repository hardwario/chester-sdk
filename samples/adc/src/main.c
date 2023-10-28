/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_adc.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A0);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A1);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A2);

	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A3);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B0);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B1);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B2);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B3);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		uint16_t sample;

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A0, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample A0: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A1, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample A1: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A2, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample A2: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A3, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample A3: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B0, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample B0: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B1, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample B1: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B2, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample B2: %u mV", CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B3, &sample);
		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			k_oops();
		}

		LOG_INF("Sample B3: %u mV", CTR_ADC_MILLIVOLTS(sample));

		k_sleep(K_MSEC(500));
	}

	return 0;
}
