/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_x3.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_x3

LOG_MODULE_REGISTER(ctr_x3, CONFIG_CTR_X3_LOG_LEVEL);

struct ctr_x3_config {
	const struct device *adc0_dev;
	const struct device *adc1_dev;
	const struct gpio_dt_spec pwr0_spec;
	const struct gpio_dt_spec pwr1_spec;
};

struct ctr_x3_data {
};

static inline const struct ctr_x3_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x3_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x3_set_power_(const struct device *dev, enum ctr_x3_channel channel, bool enabled)
{
	int ret;

	const struct gpio_dt_spec *spec;

	switch (channel) {
	case CTR_X3_CHANNEL_1:
		spec = &get_config(dev)->pwr0_spec;
		break;
	case CTR_X3_CHANNEL_2:
		spec = &get_config(dev)->pwr1_spec;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	ret = gpio_pin_set_dt(spec, enabled ? 1 : 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_x3_measure_(const struct device *dev, enum ctr_x3_channel channel, int32_t *result)
{
	int ret;

	const struct device *adc_dev;

	switch (channel) {
	case CTR_X3_CHANNEL_1:
		adc_dev = get_config(dev)->adc0_dev;
		break;
	case CTR_X3_CHANNEL_2:
		adc_dev = get_config(dev)->adc1_dev;
		break;
	default:
		LOG_ERR("Unknown channel: %d", channel);
		return -EINVAL;
	}

	const struct adc_sequence sequence = {
		.channels = BIT(0),
		.buffer = result,
		.buffer_size = sizeof(*result),
		.resolution = 24,
	};

	ret = adc_read(adc_dev, &sequence);
	if (ret) {
		LOG_ERR("Call `adc_read` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_x3_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

	if (get_config(dev)->adc0_dev == NULL && get_config(dev)->adc1_dev == NULL) {
		return 0;
	}

	if (!device_is_ready(get_config(dev)->pwr0_spec.port)) {
		LOG_ERR("Device not ready (pwr0)");
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->pwr1_spec.port)) {
		LOG_ERR("Device not ready (pwr1)");
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->adc0_dev)) {
		LOG_ERR("Device not ready (adc0)");
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->adc1_dev)) {
		LOG_ERR("Device not ready (adc1)");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->pwr0_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed (pwr0): %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->pwr1_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed (pwr1): %d", ret);
		return ret;
	}

	struct adc_channel_cfg cfg = {
		.gain = ADC_GAIN_1,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME_DEFAULT,
		.channel_id = BIT(0),
		.differential = 1,
	};

	ret = adc_channel_setup(get_config(dev)->adc0_dev, &cfg);
	if (ret) {
		LOG_ERR("Call `adc_channel_setup` failed (adc0): %d", ret);
		return ret;
	}

	ret = adc_channel_setup(get_config(dev)->adc1_dev, &cfg);
	if (ret) {
		LOG_ERR("Call `adc_channel_setup` failed (adc1): %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_x3_driver_api ctr_x3_driver_api = {
	.set_power = ctr_x3_set_power_,
	.measure = ctr_x3_measure_,
};

/* TODO Delete (these macros will make it to mainline) */
#define DT_INST_ENUM_IDX(inst, prop) DT_ENUM_IDX(DT_DRV_INST(inst), prop)
#define DEVICE_DT_GET_OR_NULL(node_id)                                                             \
	COND_CODE_1(DT_NODE_HAS_STATUS(node_id, okay), (DEVICE_DT_GET(node_id)), (NULL))

#define CTR_X3_INIT(n)                                                                             \
	static const struct ctr_x3_config inst_##n##_config = {                                    \
		.adc0_dev = COND_CODE_0(DT_INST_ENUM_IDX(n, slot),                                 \
					DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_ads122c04_a1)),  \
					DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_ads122c04_b1))), \
		.adc1_dev = COND_CODE_0(DT_INST_ENUM_IDX(n, slot),                                 \
					DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_ads122c04_a2)),  \
					DEVICE_DT_GET_OR_NULL(DT_NODELABEL(ctr_x3_ads122c04_b2))), \
		.pwr0_spec = GPIO_DT_SPEC_INST_GET(n, pwr0_gpios),                                 \
		.pwr1_spec = GPIO_DT_SPEC_INST_GET(n, pwr1_gpios),                                 \
	};                                                                                         \
	static struct ctr_x3_data inst_##n##_data = {};                                            \
	DEVICE_DT_INST_DEFINE(n, ctr_x3_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY, &ctr_x3_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X3_INIT)
