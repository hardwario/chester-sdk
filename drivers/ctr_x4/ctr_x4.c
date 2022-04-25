/* CHESTER includes */
#include <drivers/ctr_x4.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#define DT_DRV_COMPAT hardwario_ctr_x4

LOG_MODULE_REGISTER(ctr_x4, CONFIG_CTR_X4_LOG_LEVEL);

struct ctr_x4_config {
	const struct gpio_dt_spec pwr1_spec;
	const struct gpio_dt_spec pwr2_spec;
	const struct gpio_dt_spec pwr3_spec;
	const struct gpio_dt_spec pwr4_spec;
};

struct ctr_x4_data {
	const struct device *dev;
};

static inline const struct ctr_x4_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_x4_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int ctr_x4_set_output_(const struct device *dev, enum ctr_x4_output output, bool is_on)
{
	int ret;

#define SET_OUTPUT(ch)                                                                             \
	do {                                                                                       \
		if (output == CTR_X4_OUTPUT_##ch) {                                                \
			if (!device_is_ready(get_config(dev)->pwr##ch##_spec.port)) {              \
				LOG_ERR("Device not ready");                                       \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = gpio_pin_set_dt(&get_config(dev)->pwr##ch##_spec, is_on ? 1 : 0);    \
			if (ret) {                                                                 \
				LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);                 \
				return ret;                                                        \
			}                                                                          \
		}                                                                                  \
	} while (0)

	SET_OUTPUT(1);
	SET_OUTPUT(2);
	SET_OUTPUT(3);
	SET_OUTPUT(4);

#undef SET_OUTPUT

	return 0;
}

static int ctr_x4_init(const struct device *dev)
{
	int ret;

	LOG_INF("System initialization");

#define CHECK_READY(name)                                                                          \
	do {                                                                                       \
		if (!device_is_ready(get_config(dev)->name##_spec.port)) {                         \
			LOG_ERR("Device not ready");                                               \
			return -EINVAL;                                                            \
		}                                                                                  \
	} while (0)

	CHECK_READY(pwr1);
	CHECK_READY(pwr2);
	CHECK_READY(pwr3);
	CHECK_READY(pwr4);

#undef CHECK_READY

#define SETUP_OUTPUT(name)                                                                         \
	do {                                                                                       \
		ret = gpio_pin_configure_dt(&get_config(dev)->name##_spec, GPIO_OUTPUT);           \
		if (ret) {                                                                         \
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);                   \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	SETUP_OUTPUT(pwr1);
	SETUP_OUTPUT(pwr2);
	SETUP_OUTPUT(pwr3);
	SETUP_OUTPUT(pwr4);

#undef SETUP_OUTPUT

	return 0;
}

static const struct ctr_x4_driver_api ctr_x4_driver_api = {
	.set_output = ctr_x4_set_output_,
};

#define CTR_X4_INIT(n)                                                                             \
	static const struct ctr_x4_config inst_##n##_config = {                                    \
		.pwr1_spec = GPIO_DT_SPEC_INST_GET(n, pwr1_gpios),                                 \
		.pwr2_spec = GPIO_DT_SPEC_INST_GET(n, pwr2_gpios),                                 \
		.pwr3_spec = GPIO_DT_SPEC_INST_GET(n, pwr3_gpios),                                 \
		.pwr4_spec = GPIO_DT_SPEC_INST_GET(n, pwr4_gpios),                                 \
	};                                                                                         \
	static struct ctr_x4_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_x4_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
	                      POST_KERNEL, CONFIG_CTR_X4_INIT_PRIORITY, &ctr_x4_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_X4_INIT)
