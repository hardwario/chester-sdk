#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <nrfx_gpiote.h>
#include <nrfx_timer.h>
#include <hal/nrf_timer.h>
#include <nrfx_ppi.h>
#include <hal/nrf_gpio.h>

#include <stdlib.h>

#include <chester/drivers/ctr_x0.h>
#include <chester/drivers/mb7066.h>

#define DT_DRV_COMPAT maxbotix_mb7066

LOG_MODULE_REGISTER(MB7066, CONFIG_MB7066_LOG_LEVEL);

#define CHECKOUT(fn, ...) \
	do { \
		nrfx_err_t err = fn(__VA_ARGS__); \
		if (err != NRFX_SUCCESS) { \
			LOG_ERR(#fn ": 0x%x", err); \
			return 1; \
		} \
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
	int res;

	data->nmeasurement = 0;

	res = k_sem_take(&data->sem, K_NO_WAIT);
	if (res != 0) {
		LOG_ERR("Could not call `k_sem_take`: %d", res);
		goto END;
	}

	res = ctr_x0_set_mode(conf->ctr_x0, conf->power_chan, CTR_X0_MODE_PWR_SOURCE);
	if (res != 0) {
		LOG_ERR("Could not enable power source: %d", res);
		goto END;
	}

	/* Based on the datasheet it takes 170 ms to enable the sensor. */
	k_sleep(K_MSEC(170));

	enable_measurement(dev);

	res = k_sem_take(&data->sem, K_MSEC(1000 * CONFIG_MB7066_SAMPLE_COUNT));
	if (res != 0) {
		LOG_ERR("Measuring timed out: %d", res);
		goto END;
	}

	// code here
	const uint32_t m = get_median_measurement(data->measurements, data->nmeasurement);
	const float microseconds = m / 16e6;
	*value = microseconds / 58e-6 / 100.0f;

	disable_measurement(dev);

	res = ctr_x0_set_mode(conf->ctr_x0, conf->power_chan, CTR_X0_MODE_DEFAULT);
	if (res != 0) {
		LOG_ERR("Could not disable power source: %d", res);
		goto END;
	}

	
END:
	k_sem_give(&data->sem);

	return 0;
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
	CHECKOUT(nrfx_gpiote_input_configure,
		data->pin->pin,
		&(nrfx_gpiote_input_config_t){ },
		&(nrfx_gpiote_trigger_config_t){
			.trigger = NRFX_GPIOTE_TRIGGER_TOGGLE,
		},
		&(nrfx_gpiote_handler_config_t){
			.handler = gpiote_toggle_handler,
			.p_context = dev
		}
	);

	/* setup timer */
	data->timer = (nrfx_timer_t)NRFX_TIMER_INSTANCE(CONFIG_MB7066_TIMER);
	nrfx_timer_config_t timer_config = NRFX_TIMER_DEFAULT_CONFIG;
	timer_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
	CHECKOUT(nrfx_timer_init, &data->timer, &timer_config, NULL);

	/* setup PPI GPIOTE -> timer capture */
	CHECKOUT(nrfx_ppi_channel_alloc, &data->ppi_chan);
	CHECKOUT(nrfx_ppi_channel_assign, data->ppi_chan,
		nrfx_gpiote_in_event_addr_get(data->pin->pin),
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

#define MB7066_DEFINE(inst) \
	static struct mb7066_data mb7066_data_##inst; \
\
	static const struct mb7066_config mb7066_config_##inst = { \
		.pin_chan = DT_PROP(DT_INST(inst, maxbotix_mb7066), pin_chan) - 1, \
		.power_chan = DT_PROP(DT_INST(inst, maxbotix_mb7066), power_chan) - 1, \
		.ctr_x0 = DEVICE_DT_GET(DT_PARENT(DT_INST(inst, maxbotix_mb7066))), \
	}; \
\
	DEVICE_DT_INST_DEFINE(inst, &mb7066_init, NULL, \
		&mb7066_data_##inst, &mb7066_config_##inst, \
		POST_KERNEL, CONFIG_MB7066_INIT_PRIORITY, &mb7066_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MB7066_DEFINE)
