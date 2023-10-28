/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/*
This example shows how to use CHESTER X0 module and ADC to read analog voltages.
Two inputs are used, one with 1:10 voltage divider (CH1) and another without divider (CH2)

0 - 26 V input
#################################
CH1 (A2)
GND (A3)

0 - 3 V input (do not exceed 3V!)
#################################
CH2 (A4)
GND (A3)

*/

/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <math.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	/* Configure X0 channel 1: 0 - 26 V analog input
	The configuration CTR_X0_MODE_AI_INPUT enables 1:10 voltage divider */
	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_1, CTR_X0_MODE_AI_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A0);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	/* Configure X0 channel 2: 0 - 3 V analog input */
	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_2, CTR_X0_MODE_DEFAULT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A1);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		float sample[2];
		uint16_t adc_voltage;

		/* X0 Channel 1 */
		ret = ctr_adc_read(CTR_ADC_CHANNEL_A0, &adc_voltage);
		if (!ret) {
			sample[0] = CTR_ADC_X0_AI_MILLIVOLTS(adc_voltage) / 1000.f;
		} else {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			sample[0] = NAN;
		}

		/* X0 Channel 2 */
		ret = ctr_adc_read(CTR_ADC_CHANNEL_A1, &adc_voltage);
		if (!ret) {
			sample[1] = CTR_ADC_X0_AI_NODIV_MILLIVOLTS(adc_voltage) / 1000.f;
		} else {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			sample[1] = NAN;
		}

		LOG_INF("Voltage CH1(0-26V): %.3f V, CH2(0-3V): %.3f", sample[0], sample[1]);

		k_sleep(K_MSEC(500));
	}

	return 0;
}
