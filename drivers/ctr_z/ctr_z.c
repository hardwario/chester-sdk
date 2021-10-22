#include "ctr_z_reg.h"

#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/ctr_z.h>
#include <drivers/i2c.h>
#include <logging/log.h>
#include <sys/byteorder.h>
#include <zephyr.h>

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
	struct gpio_callback cb;
	ctr_z_event_cb user_cb;
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
	uint8_t data_[2];

	ret = i2c_burst_read(get_config(dev)->i2c_dev, get_config(dev)->i2c_addr, reg, data_,
	                     sizeof(data_));
	if (ret < 0) {
		return ret;
	}

	*data = sys_get_be16(data_);

	return 0;
}

static int write(const struct device *dev, uint8_t reg, uint16_t data)
{
	int ret;
	uint8_t data_[2];

	sys_put_be16(data, data_);

	ret = i2c_burst_write(get_config(dev)->i2c_dev, get_config(dev)->i2c_addr, reg, data_,
	                      sizeof(data_));
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static int ctr_z_set_callback_(const struct device *dev, ctr_z_event_cb callback, void *user_data)
{
	get_data(dev)->user_cb = callback;
	get_data(dev)->user_data = user_data;

	return 0;
}

static int ctr_z_apply_(const struct device *dev)
{
	int ret;

	ret = write(dev, REG_CONTROL, BIT(15));
	if (ret < 0) {
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
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	status->dc_input_connected = (bool)(reg_status & BIT(5));
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
	if (ret < 0) {
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
	if (ret < 0) {
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
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_serno1;

	ret = read(dev, REG_SERNO1, &reg_serno1);
	if (ret < 0) {
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
	if (ret < 0) {
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
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_hwvar1;

	ret = read(dev, REG_HWVAR1, &reg_hwvar1);
	if (ret < 0) {
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
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_fwver1;

	ret = read(dev, REG_FWVER1, &reg_fwver1);
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*fw_version = reg_fwver0 << 16 | reg_fwver1;

	return 0;
}

static int ctr_z_get_vendor_name_(const struct device *dev, char **vendor_name)
{
	int ret;

	static char buf[32];

	for (size_t i = 0; i < ARRAY_SIZE(buf); i++) {
		uint16_t reg_vend;

		ret = read(dev, REG_VEND0 + (uint8_t)i, &reg_vend);
		if (ret < 0) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		buf[i] = (char)reg_vend;
	}

	*vendor_name = buf;

	return 0;
}

static int ctr_z_get_product_name_(const struct device *dev, char **product_name)
{
	int ret;

	static char buf[32];

	for (size_t i = 0; i < ARRAY_SIZE(buf); i++) {
		uint16_t reg_prod;

		ret = read(dev, REG_PROD0 + (uint8_t)i, &reg_prod);
		if (ret < 0) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		buf[i] = (char)reg_prod;
	}

	*product_name = buf;

	return 0;
}

static int ctr_z_set_buzzer_(const struct device *dev, const struct ctr_z_buzzer_param *param)
{
	int ret;

	uint16_t reg_buzzer;

	reg_buzzer = (uint16_t)param->command << 4 | (uint16_t)param->pattern;

	ret = write(dev, REG_BUZZER, reg_buzzer);
	if (ret < 0) {
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
	if (ret < 0) {
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
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
	}

	ret = write(data->dev, REG_IRQ0, reg_irq0);
	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
	}

	uint16_t reg_irq1;

	ret = read(data->dev, REG_IRQ1, &reg_irq1);
	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
	}

	ret = write(data->dev, REG_IRQ1, reg_irq1);
	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
	}

	uint32_t reg_irq = reg_irq0 << 16 | reg_irq1;

	if (data->user_cb) {
		for (int idx = __builtin_ffs(reg_irq); idx != 0; idx = __builtin_ffs(reg_irq)) {
			data->user_cb(data->dev, idx - 1, data->user_data);
			WRITE_BIT(reg_irq, idx - 1, 0);
		}
	}

	ret = gpio_pin_interrupt_configure_dt(&config->irq_spec, GPIO_INT_LEVEL_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}
}

static void irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int ret;

	struct ctr_z_data *data = CONTAINER_OF(cb, struct ctr_z_data, cb);
	const struct ctr_z_config *config = get_config(data->dev);

	ret = gpio_pin_interrupt_configure_dt(&config->irq_spec, GPIO_INT_DISABLE);
	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return;
	}

	k_work_submit(&data->work);
}

static int ctr_z_init(const struct device *dev)
{
	int ret;

	k_work_init(&get_data(dev)->work, work_handler);

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("IRQ port not ready");
		return -EINVAL;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->irq_spec, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Unable to configure IRQ pin: %d", ret);
		return ret;
	}

	gpio_init_callback(&get_data(dev)->cb, irq_handler, BIT(get_config(dev)->irq_spec.pin));

	ret = gpio_add_callback(get_config(dev)->irq_spec.port, &get_data(dev)->cb);
	if (ret < 0) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->irq_spec, GPIO_INT_LEVEL_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return ret;
	}

#define INIT_REGISTER(reg, val)                                                                    \
	ret = write(dev, reg, val);                                                                \
	if (ret < 0) {                                                                             \
		LOG_WRN("Register initialization (" STRINGIFY(reg) ") failed: %d", ret);           \
	}

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

	/*
	 * Clear all pending interrupts
	 */
	INIT_REGISTER(REG_IRQ0, BIT_MASK(16));
	INIT_REGISTER(REG_IRQ1, BIT_MASK(16));

#undef INIT_REGISTER

	/*
	 * Apply register clearance, enable alert pin and enable auto-beep if configured
	 */
	ret = write(dev, REG_CONTROL, BIT(0) | (get_config(dev)->auto_beep ? BIT(1) : 0) | BIT(15));
	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static const struct ctr_z_driver_api ctr_z_driver_api = {
	.set_callback = ctr_z_set_callback_,
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
	DEVICE_DT_INST_DEFINE(n, &ctr_z_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
	                      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ctr_z_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_Z_INIT)
