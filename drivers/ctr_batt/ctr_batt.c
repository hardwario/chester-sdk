#include <devicetree.h>
#include <drivers/adc.h>
#include <drivers/ctr_batt.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

#define DT_DRV_COMPAT hardwario_ctr_batt

LOG_MODULE_REGISTER(ctr_batt, CONFIG_CTR_BATT_LOG_LEVEL);

struct ctr_batt_config {
	const struct device *adc_dev;
	struct adc_channel_cfg adc_channel_cfg;
	const struct gpio_dt_spec load_spec;
	const struct gpio_dt_spec test_spec;
	int r_load;
	int r1;
	int r2;
};

struct ctr_batt_data {
	struct k_mutex lock;
	struct adc_sequence adc_sequence;
	int16_t adc_buf[1];
};

static inline const struct ctr_batt_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_batt_data *get_data(const struct device *dev)
{
	return dev->data;
}

static inline int batt_measure_to_sys(const struct device *dev, int u)
{
	return u * (get_config(dev)->r1 + get_config(dev)->r2) / get_config(dev)->r2;
}

static int ctr_batt_get_rest_voltage_mv_(const struct device *dev, int *rest_mv,
                                         k_timeout_t timeout)
{
	int ret;

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!device_is_ready(get_config(dev)->test_spec.port)) {
		LOG_ERR("Port `TEST` not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->adc_dev)) {
		LOG_ERR("Device `ADC` not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->test_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = k_sleep(timeout);
	if (ret) {
		LOG_ERR("Waking too early: %d ms remaining", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return -ECANCELED;
	}

	ret = adc_read(get_config(dev)->adc_dev, &get_data(dev)->adc_sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->test_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);

		return ret;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	int32_t u = batt_measure_to_sys(dev, get_data(dev)->adc_buf[0]);

	ret = adc_raw_to_millivolts(adc_ref_internal(get_config(dev)->adc_dev),
	                            get_config(dev)->adc_channel_cfg.gain,
	                            get_data(dev)->adc_sequence.resolution, &u);
	if (ret) {
		LOG_ERR("Call `adc_raw_to_millivolts` failed: %d", ret);
		return ret;
	}

	*rest_mv = u;

	return 0;
}

static int ctr_batt_get_load_voltage_mv_(const struct device *dev, int *load_mv,
                                         k_timeout_t timeout)
{
	int ret;

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!device_is_ready(get_config(dev)->load_spec.port)) {
		LOG_ERR("Port `LOAD` not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->test_spec.port)) {
		LOG_ERR("Port `TEST` not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->adc_dev)) {
		LOG_ERR("Device `ADC` not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -EINVAL;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->load_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->test_spec, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = k_sleep(timeout);
	if (ret) {
		LOG_ERR("Waking too early: %d ms remaining", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return -ECANCELED;
	}

	ret = adc_read(get_config(dev)->adc_dev, &get_data(dev)->adc_sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->test_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->load_spec, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	int32_t u = batt_measure_to_sys(dev, get_data(dev)->adc_buf[0]);

	ret = adc_raw_to_millivolts(adc_ref_internal(get_config(dev)->adc_dev),
	                            get_config(dev)->adc_channel_cfg.gain,
	                            get_data(dev)->adc_sequence.resolution, &u);
	if (ret) {
		LOG_ERR("Call `adc_raw_to_millivolts` failed: %d", ret);
		return ret;
	}

	*load_mv = u;

	return 0;
}

static void ctr_batt_get_load_current_ma_(const struct device *dev, int *current_ma, int load_mv)
{
	*current_ma = load_mv / get_config(dev)->r_load;
}

static int ctr_batt_init(const struct device *dev)
{
	int ret;

	k_mutex_init(&get_data(dev)->lock);

	get_data(dev)->adc_sequence.buffer = get_data(dev)->adc_buf;
	get_data(dev)->adc_sequence.buffer_size = sizeof(get_data(dev)->adc_buf);

	if (!device_is_ready(get_config(dev)->adc_dev)) {
		LOG_ERR("ADC not ready");
		return -EINVAL;
	}

	ret = adc_channel_setup(get_config(dev)->adc_dev, &get_config(dev)->adc_channel_cfg);
	if (ret) {
		LOG_ERR("Call `adc_channel_setup` failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->load_spec.port)) {
		LOG_ERR("Port `LOAD` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->load_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->test_spec.port)) {
		LOG_ERR("Port `TEST` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->test_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_batt_driver_api ctr_batt_driver_api = {
	.get_rest_voltage_mv = ctr_batt_get_rest_voltage_mv_,
	.get_load_voltage_mv = ctr_batt_get_load_voltage_mv_,
	.get_load_current_ma = ctr_batt_get_load_current_ma_,
};

#define CTR_BATT_INIT(n)                                                                           \
	static const struct ctr_batt_config inst_##n##_config = {                                  \
		.adc_dev = DEVICE_DT_GET(DT_CHOSEN(ctr_batt_adc)),                                 \
		.adc_channel_cfg = { .gain = ADC_GAIN_1,                                           \
		                     .reference = ADC_REF_INTERNAL,                                \
		                     .acquisition_time = ADC_ACQ_TIME_DEFAULT,                     \
		                     .channel_id = BIT(0),                                         \
		                     .differential = 1 },                                          \
		.load_spec = GPIO_DT_SPEC_INST_GET(n, load_gpios),                                 \
		.test_spec = GPIO_DT_SPEC_INST_GET(n, test_gpios),                                 \
		.r_load = DT_INST_PROP(n, load_resistor),                                          \
		.r1 = DT_INST_PROP(n, divider_r1),                                                 \
		.r2 = DT_INST_PROP(n, divider_r2),                                                 \
	};                                                                                         \
	static struct ctr_batt_data inst_##n##_data = {                                            \
		.adc_sequence = { .channels = BIT(0), .resolution = 12 },                          \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, &ctr_batt_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
	                      APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY,                       \
	                      &ctr_batt_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_BATT_INIT)
