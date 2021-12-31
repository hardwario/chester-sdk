#include <devicetree.h>
#include <drivers/ctr_rfmux.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

#define DT_DRV_COMPAT hardwario_ctr_rfmux

LOG_MODULE_REGISTER(ctr_rfmux, CONFIG_CTR_RFMUX_LOG_LEVEL);

struct ctr_rfmux_config {
	const struct gpio_dt_spec rf_lte_spec;
	const struct gpio_dt_spec rf_lrw_spec;
	const struct gpio_dt_spec rf_int_spec;
	const struct gpio_dt_spec rf_ext_spec;
};

struct ctr_rfmux_data {
	const struct device *dev;
	struct k_mutex mut;
};

static inline const struct ctr_rfmux_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_rfmux_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_rfmux_set_interface_(const struct device *dev, enum ctr_rfmux_interface interface)
{
	int ret;

	k_mutex_lock(&get_data(dev)->mut, K_FOREVER);

	if (!device_is_ready(get_config(dev)->rf_lte_spec.port)) {
		LOG_ERR("Port RF_LTE not ready");
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->rf_lrw_spec.port)) {
		LOG_ERR("Port RF_LRW not ready");
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	}

	switch (interface) {
	case CTR_RFMUX_INTERFACE_NONE:
		ret = gpio_pin_set_dt(&get_config(dev)->rf_lte_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_LTE setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lrw_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_LRW setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		break;

	case CTR_RFMUX_INTERFACE_LTE:
		ret = gpio_pin_set_dt(&get_config(dev)->rf_lrw_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_LRW setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lte_spec, 1);
		if (ret) {
			LOG_ERR("Pin RF_LTE setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		break;

	case CTR_RFMUX_INTERFACE_LRW:
		ret = gpio_pin_set_dt(&get_config(dev)->rf_lte_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_LTE setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_lrw_spec, 1);
		if (ret) {
			LOG_ERR("Pin RF_LRW setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		break;

	default:
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->mut);
	return 0;
}

static int ctr_rfmux_set_antenna_(const struct device *dev, enum ctr_rfmux_antenna antenna)
{
	int ret;

	k_mutex_lock(&get_data(dev)->mut, K_FOREVER);

	if (!device_is_ready(get_config(dev)->rf_int_spec.port)) {
		LOG_ERR("Port RF_INT not ready");
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->rf_ext_spec.port)) {
		LOG_ERR("Port RF_EXT not ready");
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	}

	switch (antenna) {
	case CTR_RFMUX_ANTENNA_NONE:
		ret = gpio_pin_set_dt(&get_config(dev)->rf_int_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_INT setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_ext_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_EXT setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		break;

	case CTR_RFMUX_ANTENNA_INT:
		ret = gpio_pin_set_dt(&get_config(dev)->rf_ext_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_EXT setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_int_spec, 1);
		if (ret) {
			LOG_ERR("Pin RF_INT setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		break;

	case CTR_RFMUX_ANTENNA_EXT:
		ret = gpio_pin_set_dt(&get_config(dev)->rf_int_spec, 0);
		if (ret) {
			LOG_ERR("Pin RF_INT setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->rf_ext_spec, 1);
		if (ret) {
			LOG_ERR("Pin RF_EXT setting failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->mut);
			return ret;
		}

		break;

	default:
		k_mutex_unlock(&get_data(dev)->mut);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->mut);
	return 0;
}

static int ctr_rfmux_init(const struct device *dev)
{
	int ret;

	k_mutex_init(&get_data(dev)->mut);

	if (!device_is_ready(get_config(dev)->rf_lte_spec.port)) {
		LOG_ERR("Port RF_LTE not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_lte_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin RF_LTE configuration failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->rf_lrw_spec.port)) {
		LOG_ERR("Port RF_LRW not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_lrw_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin RF_LRW configuration failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->rf_int_spec.port)) {
		LOG_ERR("Port RF_INT not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_int_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin RF_INT configuration failed: %d", ret);
		return ret;
	}

	if (!device_is_ready(get_config(dev)->rf_ext_spec.port)) {
		LOG_ERR("Port RF_EXT not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->rf_ext_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin RF_EXT configuration failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_rfmux_driver_api ctr_rfmux_driver_api = {
	.set_interface = ctr_rfmux_set_interface_,
	.set_antenna = ctr_rfmux_set_antenna_,
};

#define CTR_RFMUX_INIT(n)                                                                          \
	static const struct ctr_rfmux_config inst_##n##_config = {                                 \
		.rf_lte_spec = GPIO_DT_SPEC_INST_GET(n, rf_lte_gpios),                             \
		.rf_lrw_spec = GPIO_DT_SPEC_INST_GET(n, rf_lrw_gpios),                             \
		.rf_int_spec = GPIO_DT_SPEC_INST_GET(n, rf_int_gpios),                             \
		.rf_ext_spec = GPIO_DT_SPEC_INST_GET(n, rf_ext_gpios),                             \
	};                                                                                         \
	static struct ctr_rfmux_data inst_##n##_data = {                                           \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_rfmux_init, NULL, &inst_##n##_data, &inst_##n##_config,       \
	                      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                     \
			      &ctr_rfmux_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_RFMUX_INIT)
