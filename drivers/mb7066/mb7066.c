/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* CHESTER includes */
#include <chester/drivers/ctr_x0.h>
#include <chester/drivers/mb7066.h>

/* NRFX includes */
#include <hal/nrf_gpio.h>
#include <hal/nrf_timer.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>
#include <nrfx_timer.h>

/* Standard includes */
#include <stdlib.h>
#include <stdint.h>

#define DT_DRV_COMPAT maxbotix_mb7066

LOG_MODULE_REGISTER(MB7066, CONFIG_MB7066_LOG_LEVEL);

#define CHECKOUT(fn, ...)                                                                          \
	do {                                                                                       \
		nrfx_err_t err = fn(__VA_ARGS__);                                                  \
		if (err != NRFX_SUCCESS) {                                                         \
			LOG_ERR(#fn ": 0x%x", err);                                                \
			return 1;                                                                  \
		}                                                                                  \
	} while (0)

struct mb7066_data {
	const struct gpio_dt_spec *pin;

	nrfx_timer_t timer;
	nrf_ppi_channel_t ppi_chan;
	uint8_t gpiote_chan;

	bool measuring;
	uint32_t measurements[CONFIG_MB7066_SAMPLE_COUNT];
	int nmeasurement;

	struct k_sem sem;
};

struct mb7066_config {
	enum ctr_x0_channel pin_chan;
	enum ctr_x0_channel power_chan;
	const struct device *ctr_x0;
};

static int enable_measurement(const struct device *dev)
{
	struct mb7066_data *data = dev->data;

	nrfx_gpiote_trigger_enable(data->pin->pin, true);
	CHECKOUT(nrfx_ppi_channel_enable, data->ppi_chan);
	nrfx_timer_enable(&data->timer);

	return 0;
}

static int disable_measurement(const struct device *dev)
{
	struct mb7066_data *data = dev->data;

	nrfx_gpiote_trigger_disable(data->pin->pin);
	CHECKOUT(nrfx_ppi_channel_disable, data->ppi_chan);
	nrfx_timer_disable(&data->timer);

	return 0;
}

static int get_median_measurement__cmp(const void *a, const void *b)
{
	const uint32_t ua = *(const uint32_t *)a;
	const uint32_t ub = *(const uint32_t *)b;

	return ua > ub ? 1 : ua < ub ? -1 : 0;
}

/* Note: sorts the array */
static uint32_t get_median_measurement(uint32_t *a, int n)
{
	qsort(a, n, sizeof(a[0]), get_median_measurement__cmp);

	return a[n / 2];
}

static void gpiote_toggle_handler(uint32_t pin, nrfx_gpiote_trigger_t action, void *user)
{
	const struct device *dev = user;
	struct mb7066_data *data = dev->data;

	/* only measure during falling edge */
	if (gpio_pin_get_dt(data->pin)) {
		return;
	}

	if (data->nmeasurement < CONFIG_MB7066_SAMPLE_COUNT) {
		data->measurements[data->nmeasurement++] =
			nrfx_timer_capture_get(&data->timer, NRF_TIMER_CC_CHANNEL0);
		return;
	}

	disable_measurement(dev);
	k_sem_give(&data->sem);
}

static int mb7066_measure_(const struct device *dev, float *value)
{
	struct mb7066_data *data = dev->data;
	const struct mb7066_config *conf = dev->config;
	int ret;
	int ret_err = 0;

	data->nmeasurement = 0;

	ret = k_sem_take(&data->sem, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("Could not call `k_sem_take`: %d", ret);
		ret_err = ret;
		goto error;
	}

	ret = ctr_x0_set_mode(conf->ctr_x0, conf->power_chan, CTR_X0_MODE_PWR_SOURCE);
	if (ret != 0) {
		LOG_ERR("Could not enable power source: %d", ret);
		ret_err = ret;
		goto error;
	}

	/* Based on the datasheet it takes 170 ms to enable the sensor. */
	k_sleep(K_MSEC(170));

	enable_measurement(dev);

	ret = k_sem_take(&data->sem, K_MSEC(250 * CONFIG_MB7066_SAMPLE_COUNT));
	if (ret != 0) {
		LOG_ERR("Measuring timed out: %d", ret);
		ret_err = ret;
		goto error_off;
	}

	const uint32_t m = get_median_measurement(data->measurements, data->nmeasurement);
	const float microseconds = m / 16e6;
	*value = microseconds / 58e-6 / 100.0f;

error_off:
	disable_measurement(dev);

	ret = ctr_x0_set_mode(conf->ctr_x0, conf->power_chan, CTR_X0_MODE_DEFAULT);
	if (ret != 0) {
		LOG_ERR("Could not disable power source: %d", ret);
	}

error:
	k_sem_give(&data->sem);

	return ret_err;
}

static int mb7066_init(const struct device *dev)
{
	LOG_INF("System initialization");
	const struct mb7066_config *conf = dev->config;
	struct mb7066_data *data = dev->data;

	int res;
	res = ctr_x0_get_spec(conf->ctr_x0, conf->pin_chan, &data->pin);
	if (res != 0) {
		LOG_ERR("Could not get dt spec from ctr_x0: %d", res);
		return res;
	}

	gpio_pin_configure_dt(data->pin, GPIO_INPUT);

	/* setup gpiote */
	CHECKOUT(nrfx_gpiote_channel_alloc, &data->gpiote_chan);
	CHECKOUT(nrfx_gpiote_input_configure, data->pin->pin, &(nrfx_gpiote_input_config_t){},
		 &(nrfx_gpiote_trigger_config_t){
			 .trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
		 },
		 &(nrfx_gpiote_handler_config_t){.handler = gpiote_toggle_handler,
						 .p_context = (void *)dev});

	/* setup timer */
	data->timer = (nrfx_timer_t)NRFX_TIMER_INSTANCE(CONFIG_MB7066_TIMER);
	uint32_t base_frequency = NRF_TIMER_BASE_FREQUENCY_GET(data->timer.p_reg);
	nrfx_timer_config_t timer_config = NRFX_TIMER_DEFAULT_CONFIG(base_frequency);
	timer_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
	CHECKOUT(nrfx_timer_init, &data->timer, &timer_config, NULL);

	/* setup PPI GPIOTE -> timer capture */
	CHECKOUT(nrfx_ppi_channel_alloc, &data->ppi_chan);
	CHECKOUT(nrfx_ppi_channel_assign, data->ppi_chan,
		 nrfx_gpiote_in_event_address_get(data->pin->pin),
		 nrfx_timer_task_address_get(&data->timer, NRF_TIMER_TASK_CAPTURE0));
	CHECKOUT(nrfx_ppi_channel_fork_assign, data->ppi_chan,
		 nrfx_timer_task_address_get(&data->timer, NRF_TIMER_TASK_CLEAR));
	disable_measurement(dev);

	nrfx_timer_enable(&data->timer);

	k_sem_init(&data->sem, 1, 1);

	return 0;
}

static const struct mb7066_driver_api mb7066_driver_api = {
	.measure = mb7066_measure_,
};

#define MB7066_DEFINE(inst)                                                                        \
	static struct mb7066_data mb7066_data_##inst;                                              \
                                                                                                   \
	static const struct mb7066_config mb7066_config_##inst = {                                 \
		.pin_chan = DT_PROP(DT_INST(inst, maxbotix_mb7066), pin_chan) - 1,                 \
		.power_chan = DT_PROP(DT_INST(inst, maxbotix_mb7066), power_chan) - 1,             \
		.ctr_x0 = DEVICE_DT_GET(DT_PARENT(DT_INST(inst, maxbotix_mb7066))),                \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, &mb7066_init, NULL, &mb7066_data_##inst,                       \
			      &mb7066_config_##inst, POST_KERNEL, CONFIG_MB7066_INIT_PRIORITY,     \
			      &mb7066_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MB7066_DEFINE)
