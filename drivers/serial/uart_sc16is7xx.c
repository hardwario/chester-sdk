/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "uart_sc16is7xx_reg.h"

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DT_DRV_COMPAT nxp_sc16is7xx

LOG_MODULE_REGISTER(sc16is7xx, CONFIG_UART_SC16IS7XX_LOG_LEVEL);

#define TX_FIFO_SIZE   64
#define RX_FIFO_SIZE   64
#define POLL_OUT_PAUSE K_MSEC(10)
#define RESET_PAUSE    K_MSEC(30)

struct sc16is7xx_config {
	const struct i2c_dt_spec i2c_spec;
	uint32_t clock_frequency;
	uint8_t prescaler;
	const struct gpio_dt_spec irq_spec;
	const struct gpio_dt_spec reset_spec;
	int reset_delay;
	bool rts_control;
	bool rts_invert;
};

struct sc16is7xx_data {
	const struct device *dev;
	struct uart_config uart_config;
	struct k_mutex lock;
	struct k_work work;
	struct gpio_callback cb;
	uart_irq_callback_user_data_t user_cb;
	void *user_data;
	bool irq_tx_enabled;
	bool irq_rx_enabled;
	uint8_t tx_buffer[1 + TX_FIFO_SIZE];

	/* General register set */
	uint8_t reg_ier;
	uint8_t reg_iir;
	uint8_t reg_fcr;
	uint8_t reg_lcr;
	uint8_t reg_mcr;
	uint8_t reg_txlvl;
	uint8_t reg_rxlvl;
	uint8_t reg_efcr;

	/* Special register set */
	uint8_t reg_dll;
	uint8_t reg_dlh;

	/* Enhanced register set */
	uint8_t reg_efr;
};

static inline const struct sc16is7xx_config *get_config(const struct device *dev)
{
	return dev->config;
}

static inline struct sc16is7xx_data *get_data(const struct device *dev)
{
	return dev->data;
}

static int read_register(const struct device *dev, uint8_t reg, uint8_t *val)
{
	int ret;

	reg <<= SC16IS7XX_REG_SHIFT;

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, &reg, 1, val, 1);
	if (ret) {
		LOG_ERR("Call `i2c_write_read_dt` failed: %d", ret);
		return ret;
	}

	LOG_DBG("R: 0x%02x V: 0x%02x", reg, *val);

	return ret;
}

static int write_register(const struct device *dev, uint8_t reg, uint8_t val)
{
	int ret;

	reg <<= SC16IS7XX_REG_SHIFT;

	uint8_t buf[] = {reg, val};

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	LOG_DBG("R: 0x%02x V: 0x%02x", reg, val);

	ret = i2c_write_dt(&get_config(dev)->i2c_spec, buf, ARRAY_SIZE(buf));
	if (ret) {
		LOG_ERR("Call `i2c_write_dt` failed: %d", ret);
		return ret;
	}

	return ret;
}

static int sc16is7xx_poll_in(const struct device *dev, unsigned char *c)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	uint8_t reg_rxlvl;
	ret = read_register(dev, SC16IS7XX_REG_RXLVL, &reg_rxlvl);
	if (ret) {
		LOG_ERR("Call `read_register` (RXLVL) failed: %d", ret);
		return -1;
	}

	if (!reg_rxlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return -1;
	}

	ret = read_register(dev, SC16IS7XX_REG_RHR, c);
	if (ret) {
		LOG_ERR("Call `read_register` (RHR) failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return -1;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void sc16is7xx_poll_out(const struct device *dev, unsigned char c)
{
	int ret;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	for (;;) {
		uint8_t reg_txlvl;
		ret = read_register(dev, SC16IS7XX_REG_TXLVL, &reg_txlvl);
		if (ret) {
			LOG_ERR("Call `read_register` (TXLVL) failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->lock);
			return;
		}

		if (!reg_txlvl) {
			k_sleep(POLL_OUT_PAUSE);
			continue;
		}

		ret = write_register(dev, SC16IS7XX_REG_THR, c);
		if (ret) {
			LOG_ERR("Call `write_register` failed: %d", ret);
			k_mutex_unlock(&get_data(dev)->lock);
			return;
		}

		break;
	}

	k_mutex_unlock(&get_data(dev)->lock);
}

static int configure_line(const struct device *dev, const struct uart_config *cfg)
{
	int ret;

	switch (cfg->data_bits) {
	case UART_CFG_DATA_BITS_5:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_LSB_BIT, 0);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_MSB_BIT, 0);
		break;
	case UART_CFG_DATA_BITS_6:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_LSB_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_MSB_BIT, 0);
		break;
	case UART_CFG_DATA_BITS_7:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_LSB_BIT, 0);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_MSB_BIT, 1);
		break;
	case UART_CFG_DATA_BITS_8:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_LSB_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_WORDLEN_MSB_BIT, 1);
		break;
	case UART_CFG_DATA_BITS_9:
		LOG_ERR("Data bits not supported: %u", cfg->data_bits);
		return -ENOTSUP;
	default:
		return -EINVAL;
	}

	switch (cfg->parity) {
	case UART_CFG_PARITY_NONE:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_ENABLE_BIT, 0);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_TYPE_BIT, 0);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_FORCE_PARITY_BIT, 0);
		break;
	case UART_CFG_PARITY_ODD:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_ENABLE_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_TYPE_BIT, 0);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_FORCE_PARITY_BIT, 0);
		break;
	case UART_CFG_PARITY_EVEN:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_ENABLE_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_TYPE_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_FORCE_PARITY_BIT, 0);
		break;
	case UART_CFG_PARITY_MARK:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_ENABLE_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_TYPE_BIT, 0);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_FORCE_PARITY_BIT, 1);
		break;
	case UART_CFG_PARITY_SPACE:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_ENABLE_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_PARITY_TYPE_BIT, 1);
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_FORCE_PARITY_BIT, 1);
		break;
	default:
		return -EINVAL;
	}

	switch (cfg->stop_bits) {
	case UART_CFG_STOP_BITS_0_5:
		LOG_ERR("Stop bits not supported: %u", cfg->stop_bits);
		return -ENOTSUP;
	case UART_CFG_STOP_BITS_1:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_STOPLEN_BIT, 0);
		break;
	case UART_CFG_STOP_BITS_1_5:
		if (cfg->data_bits != UART_CFG_DATA_BITS_5) {
			LOG_ERR("Stop bits not supported: %u", cfg->stop_bits);
			return -ENOTSUP;
		}
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_STOPLEN_BIT, 1);
		break;
	case UART_CFG_STOP_BITS_2:
		if (cfg->data_bits == UART_CFG_DATA_BITS_5) {
			LOG_ERR("Stop bits not supported: %u", cfg->stop_bits);
			return -ENOTSUP;
		}
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_STOPLEN_BIT, 1);
		break;
	default:
		return -EINVAL;
	}

	switch (cfg->flow_ctrl) {
	case UART_CFG_FLOW_CTRL_NONE:
		break;
	case UART_CFG_FLOW_CTRL_RTS_CTS:
	case UART_CFG_FLOW_CTRL_DTR_DSR:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int enable_enhanced_features(const struct device *dev)
{
	int ret;

	/* Write special value to LCR */
	ret = write_register(dev, SC16IS7XX_REG_LCR, SC16IS7XX_LCR_MAGIC);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_efr, SC16IS7XX_EFR_EFE_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_EFR, get_data(dev)->reg_efr);
	if (ret) {
		LOG_ERR("Call `write_register` (EFR) failed: %d", ret);
		return ret;
	}

	/* Restore original LCR value */
	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int enable_extra_features(const struct device *dev)
{
	int ret;

	if (get_config(dev)->rts_control) {
		WRITE_BIT(get_data(dev)->reg_efcr, SC16IS7XX_EFCR_RTS_CONTROL_BIT, 1);
	}

	if (get_config(dev)->rts_invert) {
		WRITE_BIT(get_data(dev)->reg_efcr, SC16IS7XX_EFCR_RTS_INVERT_BIT, 1);
	}

	ret = write_register(dev, SC16IS7XX_REG_EFCR, get_data(dev)->reg_efcr);
	if (ret) {
		LOG_ERR("Call `write_register` (EFCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int configure_baudrate(const struct device *dev, const struct uart_config *cfg)
{
	int ret;

	double clock_frequency = get_config(dev)->clock_frequency;
	double prescaler = get_config(dev)->prescaler;
	double baudrate = cfg->baudrate;

	uint16_t divisor = lround((clock_frequency / prescaler) / (baudrate * 16));
	get_data(dev)->reg_dll = divisor;
	get_data(dev)->reg_dlh = divisor >> 8;

	LOG_DBG("Computed divisor: %u", divisor);

	WRITE_BIT(get_data(dev)->reg_mcr, SC16IS7XX_MCR_CLOCK_DIVISOR_BIT,
		  get_config(dev)->prescaler == 4 ? 1 : 0);

	ret = write_register(dev, SC16IS7XX_REG_MCR, get_data(dev)->reg_mcr);
	if (ret) {
		LOG_ERR("Call `write_register` (MCR) failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	ret = write_register(dev, SC16IS7XX_REG_DLL, get_data(dev)->reg_dll);
	if (ret) {
		LOG_ERR("Call `write_register` (DLL) failed: %d", ret);
		return ret;
	}

	ret = write_register(dev, SC16IS7XX_REG_DLH, get_data(dev)->reg_dlh);
	if (ret) {
		LOG_ERR("Call `write_register` (DLH) failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int sc16is7xx_configure(const struct device *dev, const struct uart_config *cfg)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	ret = configure_line(dev, cfg);
	if (ret) {
		LOG_ERR("Call `configure_line` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	ret = configure_baudrate(dev, cfg);
	if (ret) {
		LOG_ERR("Call `configure_baudrate` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	get_data(dev)->uart_config = *cfg;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int sc16is7xx_config_get(const struct device *dev, struct uart_config *cfg)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	*cfg = get_data(dev)->uart_config;

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int sc16is7xx_fifo_fill(const struct device *dev, const uint8_t *data, int data_len)
{
	int ret;

	int data_len_ = MIN(data_len, RX_FIFO_SIZE);

	if (!data_len_) {
		return 0;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->reg_txlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 0;
	}

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -ENODEV;
	}

	size_t buf_len = MIN(get_data(dev)->reg_txlvl, data_len_);

	get_data(dev)->tx_buffer[0] = SC16IS7XX_REG_THR << SC16IS7XX_REG_SHIFT;
	memcpy(&get_data(dev)->tx_buffer[1], data, buf_len);

	ret = i2c_write_dt(&get_config(dev)->i2c_spec, get_data(dev)->tx_buffer, 1 + buf_len);
	if (ret) {
		LOG_ERR("Call `i2c_write_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	get_data(dev)->reg_txlvl -= buf_len;

	k_mutex_unlock(&get_data(dev)->lock);

	return buf_len;
}

static int sc16is7xx_fifo_read(const struct device *dev, uint8_t *data, const int data_len)
{
	int ret;

	int data_len_ = MIN(data_len, RX_FIFO_SIZE);

	if (!data_len_) {
		return 0;
	}

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (!get_data(dev)->reg_rxlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 0;
	}

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Device not ready");
		k_mutex_unlock(&get_data(dev)->lock);
		return -ENODEV;
	}

	size_t buf_len = MIN(get_data(dev)->reg_rxlvl, data_len_);

	uint8_t reg = SC16IS7XX_REG_RHR << SC16IS7XX_REG_SHIFT;

	ret = i2c_write_read_dt(&get_config(dev)->i2c_spec, &reg, 1, data, buf_len);
	if (ret) {
		LOG_ERR("Call `i2c_write_read_dt` failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return ret;
	}

	get_data(dev)->reg_rxlvl -= buf_len;

	k_mutex_unlock(&get_data(dev)->lock);

	return buf_len;
}

static void sc16is7xx_irq_tx_enable(const struct device *dev)
{
	int ret;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_THRI_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return;
	}

	get_data(dev)->irq_tx_enabled = true;

	k_mutex_unlock(&get_data(dev)->lock);
}

static void sc16is7xx_irq_tx_disable(const struct device *dev)
{
	int ret;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_THRI_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return;
	}

	get_data(dev)->irq_tx_enabled = false;

	k_mutex_unlock(&get_data(dev)->lock);
}

static int sc16is7xx_irq_tx_ready(const struct device *dev)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->irq_tx_enabled && get_data(dev)->reg_txlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 1;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static void sc16is7xx_irq_rx_enable(const struct device *dev)
{
	int ret;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_RHRI_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return;
	}

	get_data(dev)->irq_rx_enabled = true;

	k_mutex_unlock(&get_data(dev)->lock);
}

static void sc16is7xx_irq_rx_disable(const struct device *dev)
{
	int ret;

	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_RHRI_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		k_mutex_unlock(&get_data(dev)->lock);
		return;
	}

	get_data(dev)->irq_rx_enabled = false;

	k_mutex_unlock(&get_data(dev)->lock);
}

static int sc16is7xx_irq_rx_ready(const struct device *dev)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->irq_rx_enabled && get_data(dev)->reg_rxlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 1;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int sc16is7xx_irq_is_pending(const struct device *dev)
{
	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	if (get_data(dev)->irq_tx_enabled && get_data(dev)->reg_txlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 1;
	}

	if (get_data(dev)->irq_rx_enabled && get_data(dev)->reg_rxlvl) {
		k_mutex_unlock(&get_data(dev)->lock);
		return 1;
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 0;
}

static int sc16is7xx_irq_update(const struct device *dev)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	ret = read_register(dev, SC16IS7XX_REG_IIR, &get_data(dev)->reg_iir);
	if (ret) {
		LOG_WRN("Call `read_register` (IIR) failed: %d", ret);
	}

	ret = read_register(dev, SC16IS7XX_REG_TXLVL, &get_data(dev)->reg_txlvl);
	if (ret) {
		LOG_WRN("Call `read_register` (TXLVL) failed: %d", ret);
	}

	ret = read_register(dev, SC16IS7XX_REG_RXLVL, &get_data(dev)->reg_rxlvl);
	if (ret) {
		LOG_WRN("Call `read_register` (RXLVL) failed: %d", ret);
	}

	k_mutex_unlock(&get_data(dev)->lock);

	return 1;
}

static void sc16is7xx_irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb,
				       void *user_data)
{
	if (k_is_in_isr()) {
		return;
	}

	k_mutex_lock(&get_data(dev)->lock, K_FOREVER);

	get_data(dev)->user_cb = cb;
	get_data(dev)->user_data = user_data;

	k_mutex_unlock(&get_data(dev)->lock);
}

static void irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	int ret;

	struct sc16is7xx_data *data = CONTAINER_OF(cb, struct sc16is7xx_data, cb);

	ret = gpio_pin_interrupt_configure_dt(&get_config(data->dev)->irq_spec, GPIO_INT_DISABLE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}

	k_work_submit(&data->work);
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int configure_reset(const struct device *dev)
{
	int ret;

	if (get_config(dev)->reset_spec.port) {
		if (!device_is_ready(get_config(dev)->reset_spec.port)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&get_config(dev)->reset_spec, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
			return ret;
		}
	}

	return 0;
}

static int trigger_reset(const struct device *dev)
{
	int ret;

	if (get_config(dev)->reset_spec.port) {
		if (!device_is_ready(get_config(dev)->reset_spec.port)) {
			LOG_ERR("Device not ready");
			return -ENODEV;
		}

		ret = gpio_pin_set_dt(&get_config(dev)->reset_spec, 1);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
			return ret;
		}

		k_sleep(K_USEC(get_config(dev)->reset_delay));

		ret = gpio_pin_set_dt(&get_config(dev)->reset_spec, 0);
		if (ret) {
			LOG_ERR("Call `gpio_pin_set_dt` failed: %d", ret);
			return ret;
		}

	} else {
		uint8_t val;

		ret = read_register(dev, SC16IS7XX_REG_LCR, &val);
		if (ret) {
			LOG_ERR("Call `read_register` (LCR) failed: %d", ret);
			return ret;
		}

		WRITE_BIT(val, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 0);

		ret = write_register(dev, SC16IS7XX_REG_LCR, val);
		if (ret) {
			LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
			return ret;
		}

		ret = write_register(dev, SC16IS7XX_REG_IOCONTROL,
				     BIT(SC16IS7XX_IOCONTROL_SRESET_BIT));
	}

	k_sleep(RESET_PAUSE);

	return 0;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int configure_interrupt_pin(const struct device *dev)
{
	int ret;

	gpio_init_callback(&get_data(dev)->cb, irq_handler, BIT(get_config(dev)->irq_spec.pin));

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = gpio_add_callback(get_config(dev)->irq_spec.port, &get_data(dev)->cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->irq_spec, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Call `gpio_pin_configure_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int enable_interrupt_pin(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->irq_spec, GPIO_INT_LEVEL_ACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int enable_fifo(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_fcr, SC16IS7XX_FCR_FIFO_ENABLE_BIT, 1);

	uint8_t val = get_data(dev)->reg_fcr;

	WRITE_BIT(val, SC16IS7XX_FCR_RESET_RX_FIFO_BIT, 1);
	WRITE_BIT(val, SC16IS7XX_FCR_RESET_TX_FIFO_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_FCR, val);
	if (ret) {
		LOG_ERR("Call `write_register` (FCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int flush_fifo(const struct device *dev)
{
	int ret;

	uint8_t val = get_data(dev)->reg_fcr;

	WRITE_BIT(val, SC16IS7XX_FCR_RESET_RX_FIFO_BIT, 1);
	WRITE_BIT(val, SC16IS7XX_FCR_RESET_TX_FIFO_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_FCR, val);
	if (ret) {
		LOG_ERR("Call `write_register` (FCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static void work_handler(struct k_work *work)
{
	int ret;

	struct sc16is7xx_data *data = CONTAINER_OF(work, struct sc16is7xx_data, work);

	if (data->user_cb) {
		data->user_cb(data->dev, data->user_data);
	} else {
		ret = flush_fifo(data->dev);
		if (ret) {
			LOG_ERR("Call `flush_fifo` failed: %d", ret);
		}
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(data->dev)->irq_spec,
					      GPIO_INT_LEVEL_ACTIVE);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed: %d", ret);
	}
}

static int sc16is7xx_init(const struct device *dev)
{
	int ret;

	k_mutex_init(&get_data(dev)->lock);

	k_work_init(&get_data(dev)->work, work_handler);

	ret = configure_reset(dev);
	if (ret) {
		LOG_ERR("Call `configure_reset` failed: %d", ret);
		return ret;
	}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	ret = configure_interrupt_pin(dev);
	if (ret) {
		LOG_ERR("Call `configure_interrupt_pin` failed: %d", ret);
		return ret;
	}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

	ret = trigger_reset(dev);
	if (ret) {
		LOG_ERR("Call `trigger_reset` failed: %d", ret);
		return ret;
	}

	ret = enable_interrupt_pin(dev);
	if (ret) {
		LOG_ERR("Call `enable_interrupt_pin` failed: %d", ret);
		return ret;
	}

	ret = enable_enhanced_features(dev);
	if (ret) {
		LOG_ERR("Call `enable_enhanced_features` failed: %d", ret);
		return ret;
	}

	ret = enable_extra_features(dev);
	if (ret) {
		LOG_ERR("Call `enable_extra_features` failed: %d", ret);
		return ret;
	}

	ret = enable_fifo(dev);
	if (ret) {
		LOG_ERR("Call `enable_fifo` failed: %d", ret);
		return ret;
	}

	ret = sc16is7xx_configure(dev, &get_data(dev)->uart_config);
	if (ret) {
		LOG_ERR("Call `sc16is7xx_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM_DEVICE

static int enable_sleep_mode(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_SLEEP_MODE_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int disable_sleep_mode(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_SLEEP_MODE_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		return ret;
	}

	return 0;
}

static int pm_control_suspend(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	ret = write_register(dev, SC16IS7XX_REG_DLL, 0);
	if (ret) {
		LOG_ERR("Call `write_register` (DLL) failed: %d", ret);
		return ret;
	}

	ret = write_register(dev, SC16IS7XX_REG_DLH, 0);
	if (ret) {
		LOG_ERR("Call `write_register` (DLH) failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	ret = enable_sleep_mode(dev);
	if (ret) {
		LOG_ERR("Call `enable_sleep_mode` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int pm_control_resume(const struct device *dev)
{
	int ret;

	ret = disable_sleep_mode(dev);
	if (ret) {
		LOG_ERR("Call `disable_sleep_mode` failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	ret = write_register(dev, SC16IS7XX_REG_DLL, get_data(dev)->reg_dll);
	if (ret) {
		LOG_ERR("Call `write_register` (DLL) failed: %d", ret);
		return ret;
	}

	ret = write_register(dev, SC16IS7XX_REG_DLH, get_data(dev)->reg_dlh);
	if (ret) {
		LOG_ERR("Call `write_register` (DLH) failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_LCR, get_data(dev)->reg_lcr);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	uint8_t reg_rxlvl;
	ret = read_register(dev, SC16IS7XX_REG_RXLVL, &reg_rxlvl);
	if (ret) {
		LOG_ERR("Call `read_register` (RXLVL) failed: %d", ret);
		return ret;
	}

	k_sleep(K_USEC(5));

	return 0;
}

static int sc16is7xx_pm_control(const struct device *dev, enum pm_device_action action)
{
	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		return pm_control_suspend(dev);
	case PM_DEVICE_ACTION_RESUME:
		return pm_control_resume(dev);
	default:
		return -ENOTSUP;
	}

	return 0;
}

#endif /* CONFIG_PM_DEVICE */

static const struct uart_driver_api sc16is7xx_driver_api = {
	.poll_in = sc16is7xx_poll_in,
	.poll_out = sc16is7xx_poll_out,
	.configure = sc16is7xx_configure,
	.config_get = sc16is7xx_config_get,

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill = sc16is7xx_fifo_fill,
	.fifo_read = sc16is7xx_fifo_read,
	.irq_tx_enable = sc16is7xx_irq_tx_enable,
	.irq_tx_disable = sc16is7xx_irq_tx_disable,
	.irq_tx_ready = sc16is7xx_irq_tx_ready,
	.irq_rx_enable = sc16is7xx_irq_rx_enable,
	.irq_rx_disable = sc16is7xx_irq_rx_disable,
	.irq_rx_ready = sc16is7xx_irq_rx_ready,
	.irq_is_pending = sc16is7xx_irq_is_pending,
	.irq_update = sc16is7xx_irq_update,
	.irq_callback_set = sc16is7xx_irq_callback_set,
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
};

#define SC16IS7XX_INIT(n)                                                                          \
	static const struct sc16is7xx_config inst_##n##_config = {                                 \
		.i2c_spec = I2C_DT_SPEC_INST_GET(n),                                               \
		.clock_frequency = DT_INST_PROP(n, clock_frequency),                               \
		.prescaler = DT_INST_PROP(n, prescaler),                                           \
		.irq_spec = GPIO_DT_SPEC_INST_GET(n, irq_gpios),                                   \
		.reset_spec = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}),                       \
		.reset_delay = DT_INST_PROP(n, reset_delay),                                       \
		.rts_control = DT_INST_PROP(n, rts_control),                                       \
		.rts_invert = DT_INST_PROP(n, rts_invert),                                         \
	};                                                                                         \
	static struct sc16is7xx_data inst_##n##_data = {                                           \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.uart_config = {.baudrate = DT_INST_PROP_OR(n, current_speed, 115200),             \
				.data_bits = UART_CFG_DATA_BITS_8,                                 \
				.parity = UART_CFG_PARITY_NONE,                                    \
				.stop_bits = UART_CFG_STOP_BITS_1,                                 \
				.flow_ctrl = UART_CFG_FLOW_CTRL_NONE},                             \
	};                                                                                         \
	PM_DEVICE_DT_INST_DEFINE(n, sc16is7xx_pm_control);                                         \
	DEVICE_DT_INST_DEFINE(n, sc16is7xx_init, PM_DEVICE_DT_INST_GET(n), &inst_##n##_data,       \
			      &inst_##n##_config, POST_KERNEL,                                     \
			      CONFIG_UART_SC16IS7XX_INIT_PRIORITY, &sc16is7xx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SC16IS7XX_INIT)
