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

#define X0_CHANNEL  CTR_X0_CHANNEL_4
#define ADC_CHANNEL CTR_ADC_CHANNEL_A3 // ADC Channel is off-by-one

K_SEM_DEFINE(g_app_init_sem, 0, 1);
enum ctr_x0_mode x0_mode = CTR_X0_MODE_DEFAULT;

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

	LOG_WRN("Enter command to confgiure X0 CH%d", X0_CHANNEL + 1);
	LOG_WRN("default, npn, ai, cl, pwr");
	LOG_WRN("Start again by typing \"kernel reboot cold\"");

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
