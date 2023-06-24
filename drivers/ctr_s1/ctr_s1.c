/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_s1_reg.h"

/* CHESTER includes */
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DT_DRV_COMPAT hardwario_ctr_s1

LOG_MODULE_REGISTER(ctr_s1, CONFIG_CTR_S1_LOG_LEVEL);

#define MAX_POLL_TIME K_MSEC(5000)

struct ctr_s1_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
	const struct gpio_dt_spec irq_spec;
	bool auto_beep;
};

struct ctr_s1_data {
	const struct device *dev;
	struct k_work work;
	struct gpio_callback gpio_cb;
	ctr_s1_user_cb user_cb;
	void *user_data;
	struct k_mutex lock;
	struct k_mutex read_lock;
	struct k_poll_signal temperature_sig;
	struct k_poll_signal humidity_sig;
	struct k_poll_signal illuminance_sig;
	struct k_poll_signal altitude_sig;
	struct k_poll_signal pressure_sig;
	struct k_poll_signal co2_conc_sig;
};

static inline const struct ctr_s1_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct ctr_s1_data *get_data(const struct device *dev)
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

	return 0;
}

static int write(const struct device *dev, uint8_t reg, uint16_t data)
{
	int ret;

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

static int ctr_s1_set_handler_(const struct device *dev, ctr_s1_user_cb user_cb, void *user_data)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = user_cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void irq_handler(const struct device *dev, struct gpio_callback *gpio_cb, uint32_t pins)
{
	int ret;

	struct ctr_s1_data *data = CONTAINER_OF(gpio_cb, struct ctr_s1_data, gpio_cb);

	ret = gpio_pin_interrupt_configure_dt(&get_config(data->dev)->irq_spec, GPIO_INT_DISABLE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return;
	}

	k_work_submit(&data->work);
}

static int ctr_s1_enable_interrupts_(const struct device *dev)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->irq_spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Pin `IRQ` configuration failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	gpio_init_callback(&get_data(dev)->gpio_cb, irq_handler,
			   BIT(get_config(dev)->irq_spec.pin));

	ret = gpio_add_callback(get_config(dev)->irq_spec.port, &get_data(dev)->gpio_cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->irq_spec, GPIO_INT_LEVEL_ACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int ctr_s1_apply_(const struct device *dev)
{
	int ret;

	ret = write(dev, REG_CONTROL, BIT(0) | (get_config(dev)->auto_beep ? BIT(1) : 0) | BIT(15));
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_s1_get_status_(const struct device *dev, struct ctr_s1_status *status)
{
	int ret;

	uint16_t reg_status;
	ret = read(dev, REG_STATUS, &reg_status);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	status->button_pressed = reg_status & BIT(4) ? true : false;

	return 0;
}

static int ctr_s1_get_serial_number_(const struct device *dev, uint32_t *serial_number)
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

static int ctr_s1_get_hw_revision_(const struct device *dev, uint16_t *hw_revision)
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

static int ctr_s1_get_hw_variant_(const struct device *dev, uint32_t *hw_variant)
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

static int ctr_s1_get_fw_version_(const struct device *dev, uint32_t *fw_version)
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

static int ctr_s1_get_vendor_name_(const struct device *dev, char *buf, size_t buf_size)
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

static int ctr_s1_get_product_name_(const struct device *dev, char *buf, size_t buf_size)
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

static int ctr_s1_set_buzzer_(const struct device *dev, const struct ctr_s1_buzzer_param *param)
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

static int ctr_s1_set_led_(const struct device *dev, enum ctr_s1_led_channel channel,
			   const struct ctr_s1_led_param *param)
{
	int ret;

	uint16_t reg_led = (uint16_t)param->brightness << 8 | (uint16_t)param->command << 4 |
			   (uint16_t)param->pattern;

	ret = write(dev, REG_LEDR + (uint8_t)channel, reg_led);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_s1_set_motion_sensitivity_(const struct device *dev,
					  enum ctr_s1_motion_sensitivity motion_sensitivity)
{
	int ret;

	ret = write(dev, REG_PIRSENS, motion_sensitivity);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_s1_set_motion_blind_time_(const struct device *dev,
					 enum ctr_s1_motion_blind_time motion_blind_time)
{
	int ret;

	ret = write(dev, REG_PIRBLTIME, motion_blind_time);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_s1_read_motion_count_(const struct device *dev, int *motion_count)
{
	int ret;

	*motion_count = 0;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	uint16_t reg_pircount0;
	ret = read(dev, REG_PIRCOUNT0, &reg_pircount0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	uint16_t reg_pircount1;
	ret = read(dev, REG_PIRCOUNT1, &reg_pircount1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	*motion_count = reg_pircount1 << 16 | reg_pircount0;

	return 0;
}

#define WAIT_FOR_SIGNAL(sig)                                                                       \
	do {                                                                                       \
		struct k_poll_event events[] = {                                                   \
			K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,      \
						 sig),                                             \
		};                                                                                 \
		ret = k_poll(events, ARRAY_SIZE(events), MAX_POLL_TIME);                           \
		if (ret == -EAGAIN) {                                                              \
			LOG_INF("Measurement timed out");                                          \
			k_mutex_unlock(&get_data(dev)->read_lock);                                 \
			return ret;                                                                \
		} else if (ret) {                                                                  \
			LOG_ERR("Call `k_poll` failed: %d", ret);                                  \
			k_mutex_unlock(&get_data(dev)->read_lock);                                 \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

static int ctr_s1_read_temperature_(const struct device *dev, float *temperature)
{
	int ret;

	*temperature = NAN;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->read_lock, K_FOREVER);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -ENODEV;
	}

	k_poll_signal_reset(&get_data(dev)->temperature_sig);

	/* Start measurement */
	ret = write(dev, REG_MEASURE, 0x0002);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	WAIT_FOR_SIGNAL(&get_data(dev)->temperature_sig);

	/* Read converted data */
	int16_t reg_temperature;
	ret = read(dev, REG_TEMPERATURE, &reg_temperature);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	if (reg_temperature == INT16_MAX) {
		LOG_ERR("Measurement error");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->read_lock);

	*temperature = reg_temperature / 100.f;

	return 0;
}

static int ctr_s1_read_humidity_(const struct device *dev, float *humidity)
{
	int ret;

	*humidity = NAN;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->read_lock, K_FOREVER);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -ENODEV;
	}

	k_poll_signal_reset(&get_data(dev)->humidity_sig);

	/* Start measurement */
	ret = write(dev, REG_MEASURE, 0x0004);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	WAIT_FOR_SIGNAL(&get_data(dev)->humidity_sig);

	/* Read converted data */
	int16_t reg_humidity;
	ret = read(dev, REG_HUMIDITY, &reg_humidity);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}
	if (reg_humidity == UINT16_MAX) {
		LOG_ERR("Measurement error");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->read_lock);

	*humidity = reg_humidity / 100.f;

	return 0;
}

static int ctr_s1_read_illuminance_(const struct device *dev, float *illuminance)
{
	int ret;

	*illuminance = NAN;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->read_lock, K_FOREVER);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -ENODEV;
	}

	k_poll_signal_reset(&get_data(dev)->illuminance_sig);

	/* Start measurement */
	ret = write(dev, REG_MEASURE, 0x0008);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	WAIT_FOR_SIGNAL(&get_data(dev)->illuminance_sig);

	/* Read converted data */
	uint16_t reg_illum0;
	ret = read(dev, REG_ILLUM0, &reg_illum0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	uint16_t reg_illum1;
	ret = read(dev, REG_ILLUM1, &reg_illum1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	if (reg_illum0 == UINT16_MAX && reg_illum1 == UINT16_MAX) {
		LOG_ERR("Measurement error");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->read_lock);

	*illuminance = reg_illum1 << 16 | reg_illum0;

	return 0;
}

static int ctr_s1_read_altitude_(const struct device *dev, float *altitude)
{
	int ret;

	*altitude = NAN;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->read_lock, K_FOREVER);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -ENODEV;
	}

	k_poll_signal_reset(&get_data(dev)->altitude_sig);

	/* Start measurement */
	ret = write(dev, REG_MEASURE, 0x0020);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	WAIT_FOR_SIGNAL(&get_data(dev)->altitude_sig);

	/* Read converted data */
	int16_t reg_altitude;
	ret = read(dev, REG_ALTITUDE, &reg_altitude);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	if (reg_altitude == INT16_MAX) {
		LOG_ERR("Measurement error");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->read_lock);

	*altitude = reg_altitude;

	return 0;
}

static int ctr_s1_read_pressure_(const struct device *dev, float *pressure)
{
	int ret;

	*pressure = NAN;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->read_lock, K_FOREVER);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -ENODEV;
	}

	k_poll_signal_reset(&get_data(dev)->pressure_sig);

	/* Start measurement */
	ret = write(dev, REG_MEASURE, 0x0010);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	WAIT_FOR_SIGNAL(&get_data(dev)->pressure_sig);

	/* Read converted data */
	uint16_t reg_pressure0;
	ret = read(dev, REG_PRESSURE0, &reg_pressure0);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	uint16_t reg_pressure1;
	ret = read(dev, REG_PRESSURE1, &reg_pressure1);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	if (reg_pressure0 == UINT16_MAX && reg_pressure1 == UINT16_MAX) {
		LOG_ERR("Measurement error");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->read_lock);

	*pressure = reg_pressure1 << 16 | reg_pressure0;

	return 0;
}

static int ctr_s1_read_co2_conc_(const struct device *dev, float *co2_conc)
{
	int ret;

	*co2_conc = NAN;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->read_lock, K_FOREVER);

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -ENODEV;
	}

	k_poll_signal_reset(&get_data(dev)->co2_conc_sig);

	/* Start measurement */
	ret = write(dev, REG_MEASURE, 0x0001);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	WAIT_FOR_SIGNAL(&get_data(dev)->co2_conc_sig);

	/* Read converted data */
	uint16_t reg_co2conc;
	ret = read(dev, REG_CO2CONC, &reg_co2conc);
	if (ret) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->read_lock);
		return ret;
	}

	if (reg_co2conc == UINT16_MAX) {
		LOG_ERR("Measurement error");
		k_mutex_unlock(&get_data(dev)->read_lock);
		return -EINVAL;
	}

	k_mutex_unlock(&get_data(dev)->read_lock);

	*co2_conc = reg_co2conc;

	return 0;
}

#undef WAIT_FOR_SIGNAL

static int ctr_s1_calib_tgt_co2_conc_(const struct device *dev, float tgt_co2_conc)
{
	int ret;

	uint16_t reg_co2caltgt = tgt_co2_conc;

	ret = write(dev, REG_CO2CALTGT, reg_co2caltgt);
	if (ret) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void work_handler(struct k_work *work)
{
	int ret;

	struct ctr_s1_data *data = CONTAINER_OF(work, struct ctr_s1_data, work);

	k_mutex_lock(&data->lock, K_FOREVER);

	uint16_t reg_irq0;
	ret = read(data->dev, REG_IRQ0, &reg_irq0);
	if (ret) {
		k_mutex_unlock(&data->lock);
		LOG_ERR("Call `read` failed: %d", ret);
	}

	ret = write(data->dev, REG_IRQ0, reg_irq0);
	if (ret) {
		k_mutex_unlock(&data->lock);
		LOG_ERR("Call `write` failed: %d", ret);
	}

#define DISPATCH(event)                                                                            \
	do {                                                                                       \
		if (reg_irq & BIT(event)) {                                                        \
			data->user_cb(data->dev, event, data->user_data);                          \
		}                                                                                  \
	} while (0);

	if (data->user_cb) {
		uint32_t reg_irq = reg_irq0;

		DISPATCH(CTR_S1_EVENT_DEVICE_RESET);
		DISPATCH(CTR_S1_EVENT_BUTTON_PRESSED);
		DISPATCH(CTR_S1_EVENT_BUTTON_CLICKED);
		DISPATCH(CTR_S1_EVENT_BUTTON_HOLD);
		DISPATCH(CTR_S1_EVENT_BUTTON_RELEASED);
		DISPATCH(CTR_S1_EVENT_MOTION_DETECTED);
		DISPATCH(CTR_S1_EVENT_CO2_TGT_CALIB_DONE);
	}

#undef DISPATCH

	k_mutex_unlock(&data->lock);

	if (reg_irq0 & BIT(CTR_S1_EVENT_TEMPERATURE_CONVERTED)) {
		k_poll_signal_raise(&data->temperature_sig, 0);
	}

	if (reg_irq0 & BIT(CTR_S1_EVENT_HUMIDITY_CONVERTED)) {
		k_poll_signal_raise(&data->humidity_sig, 0);
	}

	if (reg_irq0 & BIT(CTR_S1_EVENT_ILLUMINANCE_CONVERTED)) {
		k_poll_signal_raise(&data->illuminance_sig, 0);
	}

	if (reg_irq0 & BIT(CTR_S1_EVENT_ALTITUDE_CONVERTED)) {
		k_poll_signal_raise(&data->altitude_sig, 0);
	}

	if (reg_irq0 & BIT(CTR_S1_EVENT_PRESSURE_CONVERTED)) {
		k_poll_signal_raise(&data->pressure_sig, 0);
	}

	if (reg_irq0 & BIT(CTR_S1_EVENT_CO2_CONC_CONVERTED)) {
		k_poll_signal_raise(&data->co2_conc_sig, 0);
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(data->dev)->irq_spec,
					      GPIO_INT_LEVEL_ACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}
}

static int ctr_s1_init(const struct device *dev)
{
	int ret;

	k_work_init(&get_data(dev)->work, work_handler);

	k_poll_signal_init(&get_data(dev)->temperature_sig);
	k_poll_signal_init(&get_data(dev)->humidity_sig);
	k_poll_signal_init(&get_data(dev)->illuminance_sig);
	k_poll_signal_init(&get_data(dev)->altitude_sig);
	k_poll_signal_init(&get_data(dev)->pressure_sig);
	k_poll_signal_init(&get_data(dev)->co2_conc_sig);

	k_mutex_init(&get_data(dev)->lock);
	k_mutex_init(&get_data(dev)->read_lock);

	if (!device_is_ready(get_config(dev)->i2c_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
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
		INIT_REGISTER(REG_LEDR, 0);
		INIT_REGISTER(REG_LEDG, 0);
		INIT_REGISTER(REG_LEDB, 0);

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

static const struct ctr_s1_driver_api ctr_s1_driver_api = {
	.set_handler = ctr_s1_set_handler_,
	.enable_interrupts = ctr_s1_enable_interrupts_,
	.apply = ctr_s1_apply_,
	.get_status = ctr_s1_get_status_,
	.get_serial_number = ctr_s1_get_serial_number_,
	.get_hw_revision = ctr_s1_get_hw_revision_,
	.get_hw_variant = ctr_s1_get_hw_variant_,
	.get_fw_version = ctr_s1_get_fw_version_,
	.get_vendor_name = ctr_s1_get_vendor_name_,
	.get_product_name = ctr_s1_get_product_name_,
	.set_buzzer = ctr_s1_set_buzzer_,
	.set_led = ctr_s1_set_led_,
	.set_motion_sensitivity = ctr_s1_set_motion_sensitivity_,
	.set_motion_blind_time = ctr_s1_set_motion_blind_time_,
	.read_motion_count = ctr_s1_read_motion_count_,
	.read_temperature = ctr_s1_read_temperature_,
	.read_humidity = ctr_s1_read_humidity_,
	.read_illuminance = ctr_s1_read_illuminance_,
	.read_altitude = ctr_s1_read_altitude_,
	.read_pressure = ctr_s1_read_pressure_,
	.read_co2_conc = ctr_s1_read_co2_conc_,
	.calib_tgt_co2_conc = ctr_s1_calib_tgt_co2_conc_,
};

#define CTR_S1_INIT(n)                                                                             \
	static const struct ctr_s1_config inst_##n##_config = {                                    \
		.i2c_dev = DEVICE_DT_GET(DT_INST_BUS(n)),                                          \
		.i2c_addr = DT_INST_REG_ADDR(n),                                                   \
		.irq_spec = GPIO_DT_SPEC_INST_GET(n, irq_gpios),                                   \
		.auto_beep = DT_INST_PROP(n, auto_beep),                                           \
	};                                                                                         \
	static struct ctr_s1_data inst_##n##_data = {                                              \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ctr_s1_init, NULL, &inst_##n##_data, &inst_##n##_config,          \
			      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY, &ctr_s1_driver_api);

DT_INST_FOREACH_STATUS_OKAY(CTR_S1_INIT)
