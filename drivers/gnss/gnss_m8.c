/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/m8.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT u_blox_m8

LOG_MODULE_REGISTER(m8, CONFIG_GNSS_M8_LOG_LEVEL);

struct m8_config {
	const struct i2c_dt_spec i2c_spec;
	const struct gpio_dt_spec main_en_spec;
	const struct gpio_dt_spec bckp_en_spec;
};

struct m8_data {
	const struct device *dev;
};

static inline const struct m8_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct m8_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int m8_set_main_power_(const struct device *dev, bool on)
{
	int ret;

	if (!device_is_ready(get_config(dev)->main_en_spec.port)) {
		LOG_ERR("Port `MAIN POWER` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->main_en_spec, on ? 1 : 0);
	if (ret) {
		LOG_ERR("Pin `MAIN POWER` setting failed: %d", ret);
		return ret;
	}

	return 0;
}

static int m8_set_bckp_power_(const struct device *dev, bool on)
{
	int ret;

	if (!device_is_ready(get_config(dev)->bckp_en_spec.port)) {
		LOG_ERR("Port `BCKP POWER` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_set_dt(&get_config(dev)->bckp_en_spec, on ? 1 : 0);
	if (ret) {
		LOG_ERR("Pin `BCKP POWER` setting failed: %d", ret);
		return ret;
	}

	return 0;
}

static int m8_read_buffer_(const struct device *dev, void *buf, size_t buf_size, size_t *bytes_read)
{
	int ret;

	if (buf_size == 0) {
		return 0;
	}

	uint8_t available_buf[2];
	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, (uint8_t[]){0xfd}, 1, available_buf,
				sizeof(available_buf));
	if (ret) {
		LOG_ERR("Length reading failed: %d", ret);
		return ret;
	}

	uint16_t available = sys_get_be16(available_buf);
	*bytes_read = MIN(available, buf_size);

	if (available == 0) {
		return 0;
	}

	LOG_DBG("Available %" PRIu16 " byte(s), reading %zu byte(s)", available, *bytes_read);

	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, (uint8_t[]){0xff}, 1, buf, *bytes_read);
	if (ret) {
		LOG_ERR("Buffer reading failed: %d", ret);
		return ret;
	}

	return 0;
}

static int m8_init(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->main_en_spec.port)) {
		LOG_ERR("Port `MAIN POWER` not ready");
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->bckp_en_spec.port)) {
		LOG_ERR("Port `BCKP POWER` not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->main_en_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin `MAIN POWER` configuration failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->bckp_en_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Pin `BCKP POWER` configuration failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct m8_driver_api m8_driver_api = {
	.set_main_power = m8_set_main_power_,
	.set_bckp_power = m8_set_bckp_power_,
	.read_buffer = m8_read_buffer_,
};

#define M8_INIT(n)                                                                                 \
	static const struct m8_config inst_##n##_config = {                                        \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.main_en_spec = GPIO_DT_SPEC_INST_GET(n, main_en_gpios),                           \
		.bckp_en_spec = GPIO_DT_SPEC_INST_GET(n, bckp_en_gpios),                           \
	};                                                                                         \
	static struct m8_data inst_##n##_data = {                                                  \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, m8_init, NULL, &inst_##n##_data, &inst_##n##_config, POST_KERNEL, \
			      CONFIG_I2C_INIT_PRIORITY, &m8_driver_api);

DT_INST_FOREACH_STATUS_OKAY(M8_INIT)
