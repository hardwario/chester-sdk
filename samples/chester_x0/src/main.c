/* CHESTER includes */
#include <chester/ctr_adc.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

uint32_t wind_adc_angle[16] = {
	273, // 0°		North
	59,  // 22.5°
	73,  // 45°
	8,   // 67.5°
	9,   // 90°		East
	6,   // 112.5°
	20,  // 135°
	13,  // 157.5°
	35,  // 180°		South
	28,  // 202.5°
	139, // 225°
	123, // 247.5°
	800, // 270°		West
	340, // 292.5°
	493, // 315°
	187, // 337.5°
};

float my_fabs(float value)
{
	if (value < 0.f) {
		return -value;
	} else {
		return value;
	}
}

float wind_voltage_to_angle(float voltage)
{
	float smallest_difference_value = 1E8f; // Set big number
	float smallest_difference_angle = 0;

	uint32_t i;

	for (i = 0; i < 16; i++) {
		float current_difference = my_fabs(voltage - wind_adc_angle[i]);
		if (smallest_difference_value > current_difference) {
			smallest_difference_value = current_difference;
			smallest_difference_angle = 22.50f * i;
		}
	}

	return smallest_difference_angle;
}

void main2(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, CTR_X0_CHANNEL_1, CTR_X0_MODE_NPN_INPUT);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_A0);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(CTR_ADC_CHANNEL_VDD);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {

		uint16_t adc_voltage;
		/*
		ret = ctr_adc_read(CTR_ADC_CHANNEL_A0, &adc_voltage);

		float sample;

		if (!ret) {
			sample = (float)CTR_ADC_MILLIVOLTS(adc_voltage);
		} else {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			sample = 0.f; // NAN;
		}

		LOG_INF("Voltage: %.0f mV   %.0f deg", sample, wind_voltage_to_angle(sample));
*/
		ret = ctr_adc_read(CTR_ADC_CHANNEL_VDD, &adc_voltage);

		float sample;

		if (!ret) {
			sample = (float)CTR_ADC_MILLIVOLTS(adc_voltage);
		} else {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
			sample = 0.f; // NAN;
		}

		LOG_INF("VDD: %.0f mV", sample);

		k_sleep(K_MSEC(200));
	}
}

K_SEM_DEFINE(g_app_init_sem, 0, 1);
enum ctr_x0_mode x0_mode = CTR_X0_MODE_DEFAULT;

#define X0_CHANNEL  CTR_X0_CHANNEL_1
#define ADC_CHANNEL CTR_ADC_CHANNEL_A0 // ADC Channel is off-by-one

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x0_a));

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, X0_CHANNEL, x0_mode);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	LOG_WRN("Enter command to confgiure X0_A CH1");

	ret = k_sem_take(&g_app_init_sem, K_FOREVER);
	if (ret) {
		LOG_ERR("Call `k_sem_take` failed: %d", ret);
		k_oops();
	}

	ret = ctr_x0_set_mode(dev, X0_CHANNEL, x0_mode);
	if (ret) {
		LOG_ERR("Call `ctr_x0_set_mode` failed: %d", ret);
		k_oops();
	}

	ret = ctr_adc_init(ADC_CHANNEL);
	if (ret) {
		LOG_ERR("Call `ctr_adc_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {

		uint16_t sample;

		ret = ctr_adc_read(ADC_CHANNEL, &sample);

		if (ret) {
			LOG_ERR("Call `ctr_adc_read` failed: %d", ret);
		}

		if (x0_mode == CTR_X0_MODE_DEFAULT || x0_mode == CTR_X0_MODE_NPN_INPUT) {
			LOG_INF("Voltage: %.0f mV", (float)CTR_ADC_MILLIVOLTS(sample));
		}

		if (x0_mode == CTR_X0_MODE_AI_INPUT) {
			LOG_INF("Voltage: %.0f mV",
				(float)CTR_ADC_MILLIVOLTS(sample) * ((100.f + 10.f) / 10.f));
		}

		if (x0_mode == CTR_X0_MODE_CL_INPUT) {
			LOG_INF("Current: %.0f mA", CTR_ADC_X0_CL_MILLIAMPS(sample));
		}

		k_sleep(K_MSEC(200));
	}
}

static int cmd_default(const struct shell *shell, size_t argc, char **argv)
{
	x0_mode = CTR_X0_MODE_DEFAULT;
	k_sem_give(&g_app_init_sem);
	return 0;
}

static int cmd_npn(const struct shell *shell, size_t argc, char **argv)
{
	x0_mode = CTR_X0_MODE_NPN_INPUT;
	k_sem_give(&g_app_init_sem);
	return 0;
}

static int cmd_ai(const struct shell *shell, size_t argc, char **argv)
{
	x0_mode = CTR_X0_MODE_AI_INPUT;
	k_sem_give(&g_app_init_sem);
	return 0;
}

static int cmd_cl(const struct shell *shell, size_t argc, char **argv)
{
	x0_mode = CTR_X0_MODE_CL_INPUT;
	k_sem_give(&g_app_init_sem);
	return 0;
}

static int cmd_pwr(const struct shell *shell, size_t argc, char **argv)
{
	x0_mode = CTR_X0_MODE_PWR_SOURCE;
	k_sem_give(&g_app_init_sem);
	return 0;
}

SHELL_CMD_REGISTER(default, NULL, "X0 default config.", cmd_default);
SHELL_CMD_REGISTER(npn, NULL, "X0 NPN input with pull-up (PU).", cmd_npn);
SHELL_CMD_REGISTER(ai, NULL, "X0 analog input with divider (PD).", cmd_ai);
SHELL_CMD_REGISTER(cl, NULL, "X0 current loop (PD).", cmd_cl);
SHELL_CMD_REGISTER(pwr, NULL, "X0 power out (PWR).", cmd_pwr);
