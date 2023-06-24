/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_adc.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_adc, CONFIG_CTR_ADC_LOG_LEVEL);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

int ctr_adc_init(enum ctr_adc_channel channel)
{
	int ret;

	if (!device_is_ready(m_dev)) {
		return -EINVAL;
	}

	struct adc_channel_cfg channel_cfg = {
		.gain = ADC_GAIN_1_6,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.channel_id = (uint8_t)channel,
		.differential = 0,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
		.input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + (uint8_t)channel
#endif
	};

	ret = adc_channel_setup(m_dev, &channel_cfg);
	if (ret) {
		LOG_ERR("Call `adc_channel_setup` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const char *get_channel_name(enum ctr_adc_channel channel)
{
	switch (channel) {
	case CTR_ADC_CHANNEL_A0:
		return "A0";
	case CTR_ADC_CHANNEL_A1:
		return "A1";
	case CTR_ADC_CHANNEL_A2:
		return "A2";
	case CTR_ADC_CHANNEL_A3:
		return "A3";
	case CTR_ADC_CHANNEL_B0:
		return "B0";
	case CTR_ADC_CHANNEL_B1:
		return "B1";
	case CTR_ADC_CHANNEL_B2:
		return "B2";
	case CTR_ADC_CHANNEL_B3:
		return "B3";
	default:
		return "?";
	}
}

int ctr_adc_read(enum ctr_adc_channel channel, uint16_t *sample)
{
	int ret;

	struct adc_sequence sequence = {
		.options = NULL,
		.channels = BIT((uint8_t)channel),
		.buffer = sample,
		.buffer_size = sizeof(*sample),
		.resolution = 12,
		.oversampling = 4,
		.calibrate = true,
	};

	ret = adc_read(m_dev, &sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		return ret;
	}

	*sample = (int16_t)*sample < 0 ? 0 : *sample;

	LOG_DBG("Channel %s: %" PRIu16 " (0x%04" PRIx16 ")", get_channel_name(channel), *sample,
		*sample);

	return 0;
}
