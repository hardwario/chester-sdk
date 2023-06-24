/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_z_reg.h"

/* CHESTER includes */
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#define DT_DRV_COMPAT hardwario_ctr_z

LOG_MODULE_REGISTER(ctr_z, CONFIG_CTR_Z_LOG_LEVEL);

struct ctr_z_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
	const struct gpio_dt_spec irq_spec;
	bool auto_beep;
};

struct ctr_z_data {
	const struct device *dev;
	struct k_work work;
	struct gpio_callback gpio_cb;
	ctr_z_user_cb user_cb;
	void *user_data;
};

static inline const struct ctr_z_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_z_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int read(const struct device *dev, uint8_t reg, uint16_t *data)
{
	int ret;

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = i2c_write_read(get_config(dev)->i2c_dev, get_config(dev)->i2c_addr, &reg, 1, data, 2);
	if (ret) {
		LOG_ERR("Call `i2c_write_read` failed: %d", ret);
		return ret;
	}

	*data = sys_be16_to_cpu(*data);

	LOG_DBG("Register: 0x%02x Data: 0x%04x", reg, *data);

	return 0;
}

static int write(const struct device *dev, uint8_t reg, uint16_t data)
{
	int ret;

	LOG_DBG("Register: 0x%02x Data: 0x%04x", reg, data);

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	uint8_t buf[3] = {reg};
	sys_put_be16(data, &buf[1]);

	ret = i2c_write(get_config(dev)->i2c_dev, buf, sizeof(buf), get_config(dev)->i2c_addr);
	if (ret) {
		LOG_ERR("Call `i2c_write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_z_set_handler_(const struct device *dev, ctr_z_user_cb user_cb, void *user_data)
{
	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	return 0;
}

static void irq_handler(const struct device *dev, struct gpio_callback *gpio_cb, uint32_t pins)
{
	int ret;

	struct ctr_z_data *data = CONTAINER_OF(gpio_cb, struct ctr_z_data, gpio_cb);
	const struct ctr_z_config *config = get_config(data->dev);

	ret = gpio_pin_interrupt_configure_dt(&config->irq_spec, GPIO_INT_DISABLE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return;
	}

	k_work_submit(&data->work);
}

static int ctr_z_enable_interrupts_(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("Port `IRQ` not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->irq_spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Pin `IRQ` configuration failed: %d", ret);
		return ret;
	}

	gpio_init_callback(&get_data(dev)->gpio_cb, irq_handler,
			   BIT(get_config(dev)->irq_spec.pin));

	ret = gpio_add_callback(get_config(dev)->irq_spec.port, &get_data(dev)->gpio_cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->irq_spec, GPIO_INT_LEVEL_ACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_z_apply_(const struct device *dev)
{
	int ret;

	ret = write(dev, REG_CONTROL, BIT(0) | (get_config(dev)->auto_beep ? BIT(1) : 0) | BIT(15));
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_z_get_status_(const struct device *dev, struct ctr_z_status *status)
{
	int ret;

	uint16_t reg_status;
	ret = read(dev, REG_STATUS, &reg_status);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	status->dc_input_connected = (bool)(reg_status & BIT(6));

	status->button_0_pressed = (bool)(reg_status & BIT(4));
	status->button_1_pressed = (bool)(reg_status & BIT(3));
	status->button_2_pressed = (bool)(reg_status & BIT(2));
	status->button_3_pressed = (bool)(reg_status & BIT(1));
	status->button_4_pressed = (bool)(reg_status & BIT(0));

	return 0;
}

static int ctr_z_get_vdc_mv_(const struct device *dev, uint16_t *vdc)
{
	int ret;

	uint16_t reg_vdc;
	ret = read(dev, REG_VDC, &reg_vdc);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*vdc = reg_vdc;

	return 0;
}

static int ctr_z_get_vbat_mv_(const struct device *dev, uint16_t *vbat)
{
	int ret;

	uint16_t reg_vbat;
	ret = read(dev, REG_VBAT, &reg_vbat);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*vbat = reg_vbat;

	return 0;
}

static int ctr_z_get_serial_number_(const struct device *dev, uint32_t *serial_number)
{
	int ret;

	uint16_t reg_serno0;
	ret = read(dev, REG_SERNO0, &reg_serno0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_serno1;
	ret = read(dev, REG_SERNO1, &reg_serno1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*serial_number = reg_serno0 << 16 | reg_serno1;

	return 0;
}

static int ctr_z_get_hw_revision_(const struct device *dev, uint16_t *hw_revision)
{
	int ret;

	uint16_t reg_hwrev;
	ret = read(dev, REG_HWREV, &reg_hwrev);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*hw_revision = reg_hwrev;

	return 0;
}

static int ctr_z_get_hw_variant_(const struct device *dev, uint32_t *hw_variant)
{
	int ret;

	uint16_t reg_hwvar0;
	ret = read(dev, REG_HWVAR0, &reg_hwvar0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_hwvar1;
	ret = read(dev, REG_HWVAR1, &reg_hwvar1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*hw_variant = reg_hwvar0 << 16 | reg_hwvar1;

	return 0;
}

static int ctr_z_get_fw_version_(const struct device *dev, uint32_t *fw_version)
{
	int ret;

	uint16_t reg_fwver0;
	ret = read(dev, REG_FWVER0, &reg_fwver0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_fwver1;
	ret = read(dev, REG_FWVER1, &reg_fwver1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*fw_version = reg_fwver0 << 16 | reg_fwver1;

	return 0;
}

static int ctr_z_get_vendor_name_(const struct device *dev, char *buf, size_t buf_size)
{
	int ret;

	for (size_t i = 0; i < MIN(32, buf_size); i++) {
		uint16_t reg_vend;
		ret = read(dev, REG_VEND0 + (uint8_t)i, &reg_vend);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		buf[i] = reg_vend;
	}

	buf[buf_size - 1] = '\0';

	return 0;
}

static int ctr_z_get_product_name_(const struct device *dev, char *buf, size_t buf_size)
{
	int ret;

	for (size_t i = 0; i < MIN(32, buf_size); i++) {
		uint16_t reg_prod;
		ret = read(dev, REG_PROD0 + (uint8_t)i, &reg_prod);
		if (ret) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		buf[i] = reg_prod;
	}

	buf[buf_size - 1] = '\0';

	return 0;
}

static int ctr_z_set_buzzer_(const struct device *dev, const struct ctr_z_buzzer_param *param)
{
	int ret;

	uint16_t reg_buzzer = (uint16_t)param->command << 4 | (uint16_t)param->pattern;
	ret = write(dev, REG_BUZZER, reg_buzzer);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_z_set_led_(const struct device *dev, enum ctr_z_led_channel channel,
			  const struct ctr_z_led_param *param)
{
	int ret;

	uint16_t reg_led = (uint16_t)param->brightness << 8 | (uint16_t)param->command << 4 |
			   (uint16_t)param->pattern;

	ret = write(dev, REG_LED0R + (uint8_t)channel, reg_led);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void work_handler(struct k_work *work)
{
	int ret;

	struct ctr_z_data *data = CONTAINER_OF(work, struct ctr_z_data, work);
	const struct ctr_z_config *config = get_config(data->dev);

	uint16_t reg_irq0;
	ret = read(data->dev, REG_IRQ0, &reg_irq0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
	}

	ret = write(data->dev, REG_IRQ0, reg_irq0);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
	}

	uint16_t reg_irq1;
	ret = read(data->dev, REG_IRQ1, &reg_irq1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
	}

	ret = write(data->dev, REG_IRQ1, reg_irq1);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
	}

#define DISPATCH(event)                                                                            \
	do {                                                                                       \
		if (reg_irq & BIT(event)) {                                                        \
			data->user_cb(data->dev, event, data->user_data);                          \
		}                                                                                  \
	} while (0);

	if (data->user_cb) {
		uint32_t reg_irq = reg_irq0 << 16 | reg_irq1;

		DISPATCH(CTR_Z_EVENT_DEVICE_RESET);
		DISPATCH(CTR_Z_EVENT_DC_CONNECTED);
		DISPATCH(CTR_Z_EVENT_DC_DISCONNECTED);
		DISPATCH(CTR_Z_EVENT_BUTTON_0_PRESS);
		DISPATCH(CTR_Z_EVENT_BUTTON_0_CLICK);
		DISPATCH(CTR_Z_EVENT_BUTTON_0_HOLD);
		DISPATCH(CTR_Z_EVENT_BUTTON_0_RELEASE);
		DISPATCH(CTR_Z_EVENT_BUTTON_1_PRESS);
		DISPATCH(CTR_Z_EVENT_BUTTON_1_CLICK);
		DISPATCH(CTR_Z_EVENT_BUTTON_1_HOLD);
		DISPATCH(CTR_Z_EVENT_BUTTON_1_RELEASE);
		DISPATCH(CTR_Z_EVENT_BUTTON_2_PRESS);
		DISPATCH(CTR_Z_EVENT_BUTTON_2_CLICK);
		DISPATCH(CTR_Z_EVENT_BUTTON_2_HOLD);
		DISPATCH(CTR_Z_EVENT_BUTTON_2_RELEASE);
		DISPATCH(CTR_Z_EVENT_BUTTON_3_PRESS);
		DISPATCH(CTR_Z_EVENT_BUTTON_3_CLICK);
		DISPATCH(CTR_Z_EVENT_BUTTON_3_HOLD);
		DISPATCH(CTR_Z_EVENT_BUTTON_3_RELEASE);
		DISPATCH(CTR_Z_EVENT_BUTTON_4_PRESS);
		DISPATCH(CTR_Z_EVENT_BUTTON_4_CLICK);
		DISPATCH(CTR_Z_EVENT_BUTTON_4_HOLD);
		DISPATCH(CTR_Z_EVENT_BUTTON_4_RELEASE);
	}

#undef DISPATCH

	ret = gpio_pin_interrupt_configure_dt(&config->irq_spec, GPIO_INT_LEVEL_ACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}
}

static int ctr_z_init(const struct device *dev)
{
	int ret;

	k_work_init(&get_data(dev)->work, work_handler);

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	uint16_t reg_protocol;
	ret = read(dev, REG_PROTOCOL, &reg_protocol);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

#define INIT_REGISTER(reg, val)                                                                    \
	ret = write(dev, reg, val);                                                                \
	if (ret) {                                                                                 \
		LOG_WRN("Register initialization (" STRINGIFY(reg) ") failed: %d", ret);           \
	}

	if (reg_protocol < 2) {
		LOG_WRN("Legacy protocol version detected");

		INIT_REGISTER(REG_CONTROL, 0);
		INIT_REGISTER(REG_BUZZER, 0);
		INIT_REGISTER(REG_LED0R, 0);
		INIT_REGISTER(REG_LED0G, 0);
		INIT_REGISTER(REG_LED0B, 0);
		INIT_REGISTER(REG_LED1R, 0);
		INIT_REGISTER(REG_LED1G, 0);
		INIT_REGISTER(REG_LED1B, 0);
		INIT_REGISTER(REG_LED2R, 0);
		INIT_REGISTER(REG_LED2G, 0);
		INIT_REGISTER(REG_LED2B, 0);
		INIT_REGISTER(REG_LED3R, 0);
		INIT_REGISTER(REG_LED3G, 0);
		INIT_REGISTER(REG_LED3B, 0);
		INIT_REGISTER(REG_LED4R, 0);
		INIT_REGISTER(REG_LED4G, 0);
		INIT_REGISTER(REG_LED4B, 0);

		/* Clear all pending interrupts */
		INIT_REGISTER(REG_IRQ0, BIT_MASK(16));
		INIT_REGISTER(REG_IRQ1, BIT_MASK(16));

		/* Apply indicator reset, enable alert pin and enable auto-beep (if configured) */
		ret = write(dev, REG_CONTROL,
			    BIT(0) | (get_config(dev)->auto_beep ? BIT(1) : 0) | BIT(15));
		if (ret) {
			LOG_ERR("Call `write` failed: %d", ret);
			return ret;
		}

	} else {
		/* Reset device */
		ret = write(dev, REG_CONTROL, 0xffff);
		if (ret) {
			LOG_ERR("Call `write` failed: %d", ret);
			return ret;
		}

		k_sleep(K_MSEC(500));
	}

#undef INIT_REGISTER

	return 0;
}

static const struct ctr_z_driver_api ctr_z_driver_api = {
	.set_handler = ctr_z_set_handler_,
	.enable_interrupts = ctr_z_enable_interrupts_,
	.apply = ctr_z_apply_,
	.get_status = ctr_z_get_status_,
	.get_vdc_mv = ctr_z_get_vdc_mv_,
	.get_vbat_mv = ctr_z_get_vbat_mv_,
	.get_serial_number = ctr_z_get_serial_number_,
	.get_hw_revision = ctr_z_get_hw_revision_,
	.get_hw_variant = ctr_z_get_hw_variant_,
	.get_fw_version = ctr_z_get_fw_version_,
	.get_vendor_name = ctr_z_get_vendor_name_,
	.get_product_name = ctr_z_get_product_name_,
	.set_buzzer = ctr_z_set_buzzer_,
	.set_led = ctr_z_set_led_,
};

#define CTR_Z_INIT(n)                                                                              \
	static const struct ctr_z_config inst_##n##_config = {                                     \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                          \
		.i2c_addr = DT_INST_REG_ADDR(n),                                                   \
		.irq_spec = GPIO_DT_SPEC_INST_GET(n, irq_gpios),                                   \
		.auto_beep = DT_INST_PROP(n, auto_beep),                                           \
	};                                                                                         \
	static struct ctr_z_data inst_##n##_data = {                                               \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_z_init, NULL, &inst_##n##_data, &inst_##n##_config,           \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ctr_z_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_Z_INIT)
