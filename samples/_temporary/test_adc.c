#include "test_adc.h"
#include <ctr_adc.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

// Standard includes
#include <stdint.h>

LOG_MODULE_REGISTER(test_adc, LOG_LEVEL_DBG);

int test_adc(void)
{
	int ret;

	LOG_INF("Started");

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A3);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B1);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B2);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_B3);

	if (ret < 0) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		return ret;
	}

	for (;;) {
		uint16_t sample;

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A0, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample A0: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A1, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample A1: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A2, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample A2: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_A3, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample A3: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B0, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample B0: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B1, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample B1: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B2, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample B2: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		ret = ctr_adc_read(CTR_ADC_CHANNEL_B3, &sample);

		if (ret < 0) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			return ret;
		}

		LOG_INF("Sample B3: %u <0x%04x> %u mV", sample, sample, CTR_ADC_MILLIVOLTS(sample));

		k_sleep(K_MSEC(200));
	}

	return 0;
}
