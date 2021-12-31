#include "uart_sc16is7xx_reg.h"

#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/i2c.h>
#include <drivers/uart.h>
#include <logging/log.h>
#include <sys/util.h>

#include <math.h>

#define DT_DRV_COMPAT nxp_sc16is7xx

LOG_MODULE_REGISTER(sc16is7xx, CONFIG_UART_SC16IS7XX_LOG_LEVEL);

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

	/*
	 * General register set
	 */
	uint8_t reg_ier;
	uint8_t reg_iir;
	uint8_t reg_fcr;
	uint8_t reg_lcr;
	uint8_t reg_mcr;
	uint8_t reg_txlvl;
	uint8_t reg_rxlvl;
	uint8_t reg_efcr;

	/*
	 * Special register set
	 */
	uint8_t reg_dll;
	uint8_t reg_dlh;

	/*
	 * Enhanced register set
	 */
	uint8_t reg_efr;

#ifdef CONFIG_PM_DEVICE
	enum pm_device_state pm_state;
#endif /* CONFIG_PM_DEVICE */
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
	reg <<= SC16IS7XX_REG_SHIFT;

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	return i2c_write_read_dt(&get_config(dev)->i2c_spec, &reg, 1, val, 1);
}

static int write_register(const struct device *dev, uint8_t reg, uint8_t val)
{
	reg <<= SC16IS7XX_REG_SHIFT;

	uint8_t buf[] = { reg, val };

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	return i2c_write_dt(&get_config(dev)->i2c_spec, buf, ARRAY_SIZE(buf));
}

static int sc16is7xx_poll_in(const struct device *dev, unsigned char *c)
{
	return read_register(dev, SC16IS7XX_REG_RHR, c);
}

static void sc16is7xx_poll_out(const struct device *dev, unsigned char c)
{
	int ret = write_register(dev, SC16IS7XX_REG_THR, c);
	if (ret) {
		LOG_ERR("Call `poll_out` failed: %d", ret);
	}
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
		LOG_ERR("data_bits not supported: %d", cfg->data_bits);
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
		LOG_ERR("stop_bits not supported: %d", cfg->stop_bits);
		return -ENOTSUP;
	case UART_CFG_STOP_BITS_1:
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_STOPLEN_BIT, 0);
		break;
	case UART_CFG_STOP_BITS_1_5:
		if (cfg->data_bits != UART_CFG_DATA_BITS_5) {
			LOG_ERR("stop_bits not supported: %d", cfg->stop_bits);
			return -ENOTSUP;
		}
		WRITE_BIT(get_data(dev)->reg_lcr, SC16IS7XX_LCR_STOPLEN_BIT, 1);
		break;
	case UART_CFG_STOP_BITS_2:
		if (cfg->data_bits == UART_CFG_DATA_BITS_5) {
			LOG_ERR("stop_bits not supported: %d", cfg->stop_bits);
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

	/*
	 * Write special value to LCR
	 */
	ret = write_register(dev, SC16IS7XX_REG_LCR, SC16IS7XX_LCR_MAGIC);
	if (ret) {
		LOG_ERR("Call `write_register` (LCR) failed: %d", ret);
		return ret;
	}

	WRITE_BIT(get_data(dev)->reg_efr, SC16IS7XX_EFR_EFE_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_FCR, get_data(dev)->reg_efr);
	if (ret) {
		LOG_ERR("Call `write_register` (EFR) failed: %d", ret);
		return ret;
	}

	/*
	 * restore LCR
	 */
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

	float clock_frequency = get_config(dev)->clock_frequency;
	float prescaler = get_config(dev)->prescaler;
	float baudrate = cfg->baudrate;

	uint16_t divisor = roundf((clock_frequency / prescaler) / (baudrate * 16));
	get_data(dev)->reg_dll = (uint8_t)(divisor);
	get_data(dev)->reg_dlh = (uint8_t)(divisor >> 8);

	LOG_DBG("divisor: %d", divisor);

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

	ret = configure_line(dev, cfg);
	if (ret) {
		LOG_ERR("Line configuration failed: %d", ret);
		return ret;
	}

	ret = configure_baudrate(dev, cfg);
	if (ret) {
		LOG_ERR("Baud rate configuration failed: %d", ret);
		return ret;
	}

	get_data(dev)->uart_config = *cfg;

	return 0;
}

static int sc16is7xx_config_get(const struct device *dev, struct uart_config *cfg)
{
	*cfg = get_data(dev)->uart_config;

	return 0;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

static int sc16is7xx_fifo_fill(const struct device *dev, const uint8_t *data, int data_len)
{
	int ret;

	static const uint8_t reg[] = { SC16IS7XX_REG_THR << SC16IS7XX_REG_SHIFT };
	static const uint32_t reg_len = sizeof(reg);

	uint32_t buf_len = MIN(get_data(dev)->reg_txlvl, reg_len + data_len);
	uint8_t buf[buf_len];

	memcpy(buf, reg, reg_len);
	memcpy(buf + reg_len, data, buf_len - reg_len);

	struct i2c_msg msg[] = {
		{
		        .buf = buf,
		        .len = buf_len,
		        .flags = I2C_MSG_WRITE | I2C_MSG_STOP,
		},
	};

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_transfer_dt(&get_config(dev)->i2c_spec, msg, ARRAY_SIZE(msg));
	if (ret) {
		LOG_ERR("Call `i2c_transfer_dt` failed: %d", ret);
		return ret;
	}

	return buf_len - reg_len;
}

static int sc16is7xx_fifo_read(const struct device *dev, uint8_t *data, const int data_len)
{
	int ret;

	static uint8_t reg[] = { SC16IS7XX_REG_RHR << SC16IS7XX_REG_SHIFT };
	static uint32_t reg_len = sizeof(reg);

	uint32_t buf_len = MIN(get_data(dev)->reg_rxlvl, data_len);
	uint8_t *buf = (uint8_t *)data;

	struct i2c_msg msg[] = {
		{
		        .buf = reg,
		        .len = reg_len,
		        .flags = I2C_MSG_WRITE | I2C_MSG_RESTART,
		},
		{
		        .buf = buf,
		        .len = buf_len,
		        .flags = I2C_MSG_READ | I2C_MSG_STOP,
		},
	};

	if (!device_is_ready(get_config(dev)->i2c_spec.bus)) {
		LOG_ERR("Bus not ready");
		return -EINVAL;
	}

	ret = i2c_transfer_dt(&get_config(dev)->i2c_spec, msg, ARRAY_SIZE(msg));
	if (ret) {
		LOG_ERR("Call `i2c_transfer_dt` failed: %d", ret);
		return ret;
	}

	return buf_len;
}

static void sc16is7xx_irq_tx_enable(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_THRI_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		return;
	}
}

static void sc16is7xx_irq_tx_disable(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_THRI_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		return;
	}
}

static int sc16is7xx_irq_tx_ready(const struct device *dev)
{
	uint8_t iir = get_data(dev)->reg_iir & SC16IS7XX_IIR_INTERRUPT_MASK;

	switch (iir) {
	case SC16IS7XX_IIR_THRI_MASK:
		return get_data(dev)->reg_txlvl ? 1 : 0;
	default:
		return 0;
	}
}

static void sc16is7xx_irq_rx_enable(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_RHRI_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		return;
	}
}

static void sc16is7xx_irq_rx_disable(const struct device *dev)
{
	int ret;

	WRITE_BIT(get_data(dev)->reg_ier, SC16IS7XX_IER_RHRI_BIT, 0);

	ret = write_register(dev, SC16IS7XX_REG_IER, get_data(dev)->reg_ier);
	if (ret) {
		LOG_ERR("Call `write_register` (IER) failed: %d", ret);
		return;
	}
}

static int sc16is7xx_irq_rx_ready(const struct device *dev)
{
	uint8_t iir = get_data(dev)->reg_iir & SC16IS7XX_IIR_INTERRUPT_MASK;

	switch (iir) {
	case SC16IS7XX_IIR_RTOI_MASK:
	case SC16IS7XX_IIR_RHRI_MASK:
		return get_data(dev)->reg_rxlvl ? 1 : 0;
	default:
		return 0;
	}
}

static int sc16is7xx_irq_is_pending(const struct device *dev)
{
	/*
	 * Hack: It takes at least four charaters until the interrupt is issued
	 * again. If receive holding interrupts are disable earlier, there will
	 * be no interrupt indicating pending data.
	 */
	if (get_data(dev)->reg_rxlvl > 0) {
		return 1;
	}

	return get_data(dev)->reg_iir & BIT(SC16IS7XX_IIR_INTERRUPT_STATUS_BIT) ? 0 : 1;
}

static int sc16is7xx_irq_update(const struct device *dev)
{
	int ret;

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

	return 1;
}

static void sc16is7xx_irq_callback_set(const struct device *dev, uart_irq_callback_user_data_t cb,
                                       void *user_data)
{
	get_data(dev)->user_cb = cb;
	get_data(dev)->user_data = user_data;
}

static void irq_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	struct sc16is7xx_data *data = CONTAINER_OF(cb, struct sc16is7xx_data, cb);

	k_work_submit(&data->work);
}

#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int configure_reset(const struct device *dev)
{
	int ret;

	if (get_config(dev)->reset_spec.port) {
		if (!device_is_ready(get_config(dev)->reset_spec.port)) {
			LOG_ERR("nRESET port not ready");
			return -EINVAL;
		}

		ret = gpio_pin_configure_dt(&get_config(dev)->reset_spec, GPIO_OUTPUT_INACTIVE);
		if (ret) {
			LOG_ERR("Unable to configure IRQ pin: %d", ret);
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
			LOG_ERR("nRESET port not ready");
			return -EINVAL;
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
	}

	k_sleep(K_USEC(get_config(dev)->reset_delay));

	return 0;
}

static int configure_interrupt_pin(const struct device *dev)
{
	int ret;

	gpio_init_callback(&get_data(dev)->cb, irq_handler, BIT(get_config(dev)->irq_spec.pin));

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("nIRQ port not ready");
		return -EINVAL;
	}

	ret = gpio_add_callback(get_config(dev)->irq_spec.port, &get_data(dev)->cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&get_config(dev)->irq_spec, GPIO_INPUT | GPIO_INT_DEBOUNCE);
	if (ret) {
		LOG_ERR("Unable to configure IRQ pin: %d", ret);
		return ret;
	}

	return 0;
}

static int enable_interrupt_pin(const struct device *dev)
{
	int ret;

	if (!device_is_ready(get_config(dev)->irq_spec.port)) {
		LOG_ERR("nIRQ port not ready");
		return -EINVAL;
	}

	ret = gpio_pin_interrupt_configure_dt(&get_config(dev)->irq_spec, GPIO_INT_EDGE_TO_ACTIVE);
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
	WRITE_BIT(get_data(dev)->reg_fcr, SC16IS7XX_FCR_RESET_RX_FIFO_BIT, 1);
	WRITE_BIT(get_data(dev)->reg_fcr, SC16IS7XX_FCR_RESET_TX_FIFO_BIT, 1);

	ret = write_register(dev, SC16IS7XX_REG_FCR, get_data(dev)->reg_fcr);
	if (ret) {
		LOG_ERR("Call `write_register` (FCR) failed: %d", ret);
		return ret;
	}

	return 0;
}

static void work_handler(struct k_work *work)
{
	struct sc16is7xx_data *data = CONTAINER_OF(work, struct sc16is7xx_data, work);

	if (data->user_cb) {
		data->user_cb(data->dev, data->user_data);
	}
}

static int sc16is7xx_init(const struct device *dev)
{
	int ret;

#ifdef CONFIG_PM_DEVICE
	get_data(dev)->pm_state = PM_DEVICE_STATE_ACTIVE;
#endif /* CONFIG_PM_DEVICE */

	k_work_init(&get_data(dev)->work, work_handler);
	k_mutex_init(&get_data(dev)->lock);

	ret = configure_reset(dev);
	if (ret) {
		LOG_ERR("Call `configure_reset` failed: %d", ret);
		return ret;
	}

	ret = configure_interrupt_pin(dev);
	if (ret) {
		LOG_ERR("Call `configure_interrupt_pin` failed: %d", ret);
		return ret;
	}

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
		LOG_ERR("Call `configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM_DEVICE

/*
 * WAKEUP
 *
 * // Redundant??? (sleep)
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_RHR, 0x00))
 *
 * // Divisor latch
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_LCR, 0x80))
 *
 * // Baudrate
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_DLL, 0x58))
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_DLH, 0x00))
 *
 * // Disable latch and enable 8 bits data word
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_LCR, 0x03))
 *
 * // Enable data interrupt
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_RHR, 0x01))
 *
 * // Enable FIFO + Reset TX/TX
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_FCR, 0x07))
 */

/*
 * SLEEP
 *
 * // Divisor latch
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_LCR, 0x80))
 *
 * // Disable baudrate generator
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_DLL, 0x00))
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_DLH, 0x00))
 *
 * // Disable divisor latch
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_LCR, 0x03))
 *
 * // Enable Sleep mode
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_RHR, 0x10))
 *
 * // Disable FIFO + flush RX+TX
 * CHECK(_sc16is7xx_write_register(ctx, _SC16IS7XX_REG_FCR, 0x06))
 */

static int get_pm_state(const struct device *dev, enum pm_device_state *state)
{
	*state = get_data(dev)->pm_state;
	return 0;
}

static int set_pm_state(const struct device *dev, enum pm_device_state state)
{
	switch (state) {
	case PM_DEVICE_STATE_ACTIVE:
	case PM_DEVICE_STATE_LOW_POWER:
	case PM_DEVICE_STATE_SUSPEND:
	case PM_DEVICE_STATE_FORCE_SUSPEND:
	case PM_DEVICE_STATE_OFF:
	case PM_DEVICE_STATE_RESUMING:
	case PM_DEVICE_STATE_SUSPENDING:
	default:
		return 0;
	}
}

static int sc16is7xx_pm_control(const struct device *dev, uint32_t ctrl_command,
                                enum pm_device_state *state)
{
	switch (ctrl_command) {
	case PM_DEVICE_STATE_GET:
		return get_pm_state(dev, state);
	case PM_DEVICE_STATE_SET:
		return set_pm_state(dev, *state);
	default:
		return -EINVAL;
	}
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
		.reset_spec = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, { 0 }),                     \
		.reset_delay = DT_INST_PROP(n, reset_delay),                                       \
		.rts_control = DT_INST_PROP(n, rts_control),                                       \
		.rts_invert = DT_INST_PROP(n, rts_invert),                                         \
	};                                                                                         \
	static struct sc16is7xx_data inst_##n##_data = {                                           \
		.dev = DEVICE_DT_INST_GET(n),                                                      \
		.uart_config = { .baudrate = DT_INST_PROP_OR(n, current_speed, 115200),            \
		                 .data_bits = UART_CFG_DATA_BITS_8,                                \
		                 .parity = UART_CFG_PARITY_NONE,                                   \
		                 .stop_bits = UART_CFG_STOP_BITS_1,                                \
		                 .flow_ctrl = UART_CFG_FLOW_CTRL_NONE },                           \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, sc16is7xx_init, sc16is7xx_pm_control, &inst_##n##_data,           \
	                      &inst_##n##_config, POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,           \
	                      &sc16is7xx_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SC16IS7XX_INIT)
