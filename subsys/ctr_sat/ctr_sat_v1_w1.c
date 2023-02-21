/* CHESTER includes */
#include "astronode_s_messages.h"

#include <chester/ctr_sat.h>
#include <chester/ctr_w1.h>
#include <chester/drivers/w1/ds28e17.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ctr_sat_w1, CONFIG_CTR_SAT_LOG_LEVEL);

static struct ctr_w1 m_w1;

static bool m_is_initialized = false;
K_MUTEX_DEFINE(m_init_mutex);

static struct ctr_sat_w1 m_w1_shields[] = {{.ds28e17_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_0))},
					   {.ds28e17_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_1))}};
static int m_w1_shields_count = 0;
static int m_w1_shields_used = 0;

static const struct device *m_dev_ds2484 = DEVICE_DT_GET(DT_NODELABEL(ds2484));

#define SC16IS740_I2C_ADDR	0x4C
#define SC16IS7XX_FIFO_CAPACITY 64
#define SC16IS7XX_REG_SHIFT	3
#define SC16IS7XX_REG_THR	0x00
#define SC16IS7XX_REG_RHR	0x00
#define SC16IS7XX_REG_TXLVL	0x08
#define SC16IS7XX_REG_RXLVL	0x09

#define TCA9534_I2C_ADDR	   0x38
#define TCA9534_REG_OUTPUT	   0x01
#define TCA9534_REG_CONFIG	   0x03
#define TCA9534_RESET_PIN_BIT	   BIT(0)
#define TCA9534_WAKEUP_PIN_BIT	   BIT(1)
#define TCA9534_EVENT_PIN_BIT	   BIT(2)
#define TCA9534_RESET_UART_PIN_BIT BIT(3)

int ctr_sat_init_generic(struct ctr_sat *sat);

static int ctr_sat_w1_wait_for_buffer_space(struct ctr_sat *sat, int space_requirement);
static int ctr_sat_w1_get_rx_fifo_level(struct ctr_sat *sat, size_t *level);
static int ctr_sat_v1_w1_update_gpio_output(struct ctr_sat *sat);
static int ctr_sat_v1_init_w1_pca9534(struct ctr_sat *sat);
static int ctr_sat_v1_init_w1_sc16is740(struct ctr_sat *sat);
static int ctr_sat_w1_scan(void);
static int ctr_sat_v1_w1_uart_write_read(struct ctr_sat *sat);
static int ctr_sat_v1_w1_gpio_write(struct ctr_sat *sat, enum ctr_sat_pin pin, bool value);

int ctr_sat_v1_init_w1(struct ctr_sat *sat)
{
	int ret;
	int res;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&m_init_mutex, K_FOREVER);
	if (m_is_initialized == false) {
		ret = ctr_sat_w1_scan();
		if (ret) {
			LOG_DBG("Call `ctr_sat_w1_scan` failed %d.", ret);
			k_mutex_unlock(&m_init_mutex);
			return ret;
		}

		m_is_initialized = true;
	}

	if (m_w1_shields_used >= m_w1_shields_count) {
		k_mutex_unlock(&m_init_mutex);
		return -ENODEV;
	}

	int shield_id = m_w1_shields_used++;

	k_mutex_unlock(&m_init_mutex);

	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	sat_w1->ds28e17_dev = m_w1_shields[shield_id].ds28e17_dev;
	sat_w1->serial_number = m_w1_shields[shield_id].serial_number;

	ret = ctr_w1_acquire(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		res = ret;
		goto error;
	}

	if (!device_is_ready(sat_w1->ds28e17_dev)) {
		LOG_ERR("Device not ready");
		res = -ENODEV;
		goto error;
	}

	ret = ds28e17_write_config(sat_w1->ds28e17_dev, DS28E17_I2C_SPEED_100_KHZ);
	if (ret) {
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);
		res = ret;
		goto error;
	}

	ret = ctr_sat_v1_init_w1_pca9534(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_v1_init_w1_pca9534` failed %d.", ret);
		res = ret;
		goto error;
	}

	ret = ctr_sat_v1_init_w1_sc16is740(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_v1_init_w1_sc16is740` failed %d.", ret);
		res = ret;
		goto error;
	}

	ret = ctr_w1_release(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		return ret;
	}

	sat->ctr_sat_uart_write_read = ctr_sat_v1_w1_uart_write_read;
	sat->ctr_sat_gpio_write = ctr_sat_v1_w1_gpio_write;

	return ctr_sat_init_generic(sat);

error:
	ret = ctr_w1_release(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		res = res ? res : ret;
	}
	return res;
}

static int ctr_sat_v1_init_w1_pca9534(struct ctr_sat *sat)
{
	int ret;
	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	// set initial GPIO output port value
	sat_w1->gpio_state = TCA9534_RESET_PIN_BIT | TCA9534_RESET_UART_PIN_BIT;
	ret = ctr_sat_v1_w1_update_gpio_output(sat);
	if (ret) {
		LOG_ERR("Call `ctr_sat_v1_w1_update_gpio_output` failed: %d.", ret);
		return ret;
	}

	// set IO ports direction
	uint8_t buf[2];
	buf[0] = TCA9534_REG_CONFIG;
	buf[1] = ~((uint8_t)TCA9534_RESET_PIN_BIT | (uint8_t)TCA9534_RESET_UART_PIN_BIT);
	ret = ds28e17_i2c_write(sat_w1->ds28e17_dev, TCA9534_I2C_ADDR, buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d.", ret);
		return ret;
	}

	return 0;
}

static int ctr_sat_v1_init_w1_sc16is740(struct ctr_sat *sat)
{
	return 0;
}

static int ctr_sat_v1_w1_uart_write_read(struct ctr_sat *sat)
{
	int ret;
	int res;
	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	ret = ctr_w1_acquire(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		res = ret;
		goto error;
	}

	if (!device_is_ready(sat_w1->ds28e17_dev)) {
		LOG_ERR("Device not ready");
		res = -ENODEV;
		goto error;
	}

	ret = ds28e17_write_config(sat_w1->ds28e17_dev, DS28E17_I2C_SPEED_100_KHZ);
	if (ret) {
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);
		res = ret;
		goto error;
	}

#define BULK_SIZE 32

	uint8_t tx_buf[BULK_SIZE + 1];
	tx_buf[0] = SC16IS7XX_REG_THR << SC16IS7XX_REG_SHIFT;

	uint8_t *tx_rd_ptr = sat->tx_buf;

	while (sat->tx_buf_len) {
		size_t to_copy = MIN(sat->tx_buf_len, BULK_SIZE);
		memcpy(tx_buf, tx_rd_ptr, to_copy);

		ret = ctr_sat_w1_wait_for_buffer_space(sat, to_copy);
		if (ret) {
			LOG_ERR("Call `ctr_sat_w1_wait_for_buffer_space` failed %d.", ret);
			res = ret;
			goto error;
		}

		ret = ds28e17_i2c_write(sat_w1->ds28e17_dev, SC16IS740_I2C_ADDR, tx_buf,
					to_copy + 1);
		if (ret) {
			LOG_ERR("Call `ds28e17_i2c_write` failed: %d.", ret);
			res = ret;
			goto error;
		}

		tx_rd_ptr += to_copy;
		sat->tx_buf_len -= to_copy;

		LOG_DBG("Written %u byte(s) to FIFO. %u bytes remains", to_copy, sat->tx_buf_len);
	}

	LOG_DBG("TX completed. No bytes remains to send.");

	uint8_t *rx_wr_ptr = sat->rx_buf;
	int timeout = 100;

	while (true) {
		size_t received_bytes;
		ret = ctr_sat_w1_get_rx_fifo_level(sat, &received_bytes);
		if (ret) {
			LOG_ERR("Call `ctr_sat_w1_get_rx_fifo_level` failed: %d.", ret);
			res = ret;
			goto error;
		}

		if (received_bytes > 0) {

			size_t remaining_buffer_size = RX_MESSAGE_MAX_SIZE - sat->rx_buf_len;
			if (received_bytes > remaining_buffer_size) {
				res = -ENOSPC;
				goto error;
			}

			uint8_t reg = SC16IS7XX_REG_RHR << SC16IS7XX_REG_SHIFT;
			ret = ds28e17_i2c_write_read(sat_w1->ds28e17_dev, SC16IS740_I2C_ADDR, &reg,
						     1, rx_wr_ptr, received_bytes);
			if (ret) {
				LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d.", ret);
				res = ret;
				goto error;
			}

			bool contains_stop_byte = false;
			uint8_t *end = rx_wr_ptr + received_bytes;
			for (uint8_t *i = rx_wr_ptr; i < end; i++) {
				if (*i == ASTRONODE_S_END_BYTE) {
					contains_stop_byte = true;
					if (i != end - 1) {
						LOG_WRN("Stop byte was received but satelite "
							"module send "
							"additional data after stop byte!");
					}
					break;
				}
			}

			rx_wr_ptr += received_bytes;
			sat->rx_buf_len += received_bytes;

			if (contains_stop_byte) {
				res = 0;
				goto error;
			}
		} else {
			if (timeout--) {
				k_sleep(K_MSEC(1));
			} else {
				res = -ETIMEDOUT;
				goto error;
			}
		}
	}

error:
	ret = ctr_w1_release(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		res = res ? res : ret;
	}
	return res;
}

static int ctr_sat_v1_w1_update_gpio_output(struct ctr_sat *sat)
{
	int ret;
	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	uint8_t buf[2];
	buf[0] = TCA9534_REG_OUTPUT;
	buf[1] = sat_w1->gpio_state;
	ret = ds28e17_i2c_write(sat_w1->ds28e17_dev, TCA9534_I2C_ADDR, buf, sizeof(buf));
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write` failed: %d.", ret);
		return ret;
	}

	return 0;
}

static int ctr_sat_v1_w1_gpio_write(struct ctr_sat *sat, enum ctr_sat_pin pin, bool value)
{
	int ret;
	int res;
	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	if (pin == CTR_SAT_PIN_RESET) {
		WRITE_BIT(sat_w1->gpio_state, TCA9534_RESET_PIN_BIT, value);
	} else if (pin == CTR_SAT_PIN_WAKEUP) {
		WRITE_BIT(sat_w1->gpio_state, TCA9534_WAKEUP_PIN_BIT, value);
	} else {
		return -EINVAL;
	}

	ret = ctr_w1_acquire(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		res = ret;
		goto error;
	}

	if (!device_is_ready(sat_w1->ds28e17_dev)) {
		LOG_ERR("Device not ready");
		res = -ENODEV;
		goto error;
	}

	ret = ds28e17_write_config(sat_w1->ds28e17_dev, DS28E17_I2C_SPEED_100_KHZ);
	if (ret) {
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);
		res = ret;
		goto error;
	}

	ret = ctr_sat_v1_w1_update_gpio_output(sat);
	if (ret) {
		LOG_ERR("Call `ctr_sat_v1_w1_update_gpio_output` failed: %d", ret);
		res = ret;
		goto error;
	}

	res = 0;
error:
	ret = ctr_w1_release(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		res = res ? res : ret;
	}
	return res;
}

static int ctr_sat_w1_wait_for_buffer_space(struct ctr_sat *sat, int space_requirement)
{
	int ret;

	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	if (space_requirement > SC16IS7XX_FIFO_CAPACITY) {
		return -EINVAL;
	}

	int timeout = 1000; // following loop will wait for 1ms for each timeout decrement
	uint8_t val;
	do {
		uint8_t reg = SC16IS7XX_REG_TXLVL << SC16IS7XX_REG_SHIFT;

		ret = ds28e17_i2c_write_read(sat_w1->ds28e17_dev, SC16IS740_I2C_ADDR, &reg, 1, &val,
					     1);
		if (ret) {
			LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
			return ret;
		}

		if (timeout--) {
			k_sleep(K_MSEC(1));
		} else {
			return -ETIMEDOUT;
		}
	} while (val < space_requirement);

	return 0;
}

static int ctr_sat_w1_get_rx_fifo_level(struct ctr_sat *sat, size_t *level)
{
	int ret;
	uint8_t reg = SC16IS7XX_REG_RXLVL << SC16IS7XX_REG_SHIFT;
	uint8_t val;

	struct ctr_sat_w1 *sat_w1 = &sat->hw_abstraction.w1;

	ret = ds28e17_i2c_write_read(sat_w1->ds28e17_dev, SC16IS740_I2C_ADDR, &reg, 1, &val, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	*level = val;
	return 0;
}

static int ctr_sat_w1_scan_callback(struct w1_rom rom, void *user_data)
{
	int ret;

	if (rom.family != 0x19) {
		return 0;
	}

	LOG_DBG("Found 1W device which is possibly CHESTER-V1");

	uint64_t serial_number = sys_get_le48(rom.serial);

	if (m_w1_shields_count >= ARRAY_SIZE(m_w1_shields)) {
		LOG_WRN("No more space for additional device: %llu", serial_number);
		return 0;
	}

	if (!device_is_ready(m_w1_shields[m_w1_shields_count].ds28e17_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	struct w1_slave_config config = {.rom = rom};
	ret = ds28e17_set_w1_config(m_w1_shields[m_w1_shields_count].ds28e17_dev, config);
	if (ret) {
		LOG_ERR("Call `ds28e17_set_w1_config` failed: %d", ret);
		return ret;
	}

	ret = ds28e17_write_config(m_w1_shields[m_w1_shields_count].ds28e17_dev,
				   DS28E17_I2C_SPEED_100_KHZ);
	if (ret) {
		LOG_ERR("Call `ds28e17_write_config` failed: %d", ret);
		return ret;
	}

	// test existence of SC16IS740 and TCA9534 devices for identifying CHESTER-V1
	uint8_t reg = SC16IS7XX_REG_TXLVL << SC16IS7XX_REG_SHIFT;
	uint8_t val;
	ret = ds28e17_i2c_write_read(m_w1_shields[m_w1_shields_count].ds28e17_dev,
				     SC16IS740_I2C_ADDR, &reg, 1, &val, 1);
	if (ret) {
		LOG_DBG("Skipping serial number: %llu", serial_number);
		return 0;
	}

	reg = TCA9534_REG_CONFIG;
	ret = ds28e17_i2c_write_read(m_w1_shields[m_w1_shields_count].ds28e17_dev, TCA9534_I2C_ADDR,
				     &reg, 1, &val, 1);
	if (ret) {
		LOG_DBG("Skipping serial number: %llu", serial_number);
		return 0;
	}

	m_w1_shields[m_w1_shields_count].serial_number = serial_number;

	LOG_DBG("Registered serial number: %llu", serial_number);

	LOG_DBG("XXXX %p", m_w1_shields[m_w1_shields_count].ds28e17_dev);

	LOG_DBG("AAA");
	k_sleep(K_MSEC(2000));

	reg = SC16IS7XX_REG_TXLVL << SC16IS7XX_REG_SHIFT;
	ret = ds28e17_i2c_write_read(m_w1_shields[m_w1_shields_count].ds28e17_dev,
				     SC16IS740_I2C_ADDR, &reg, 1, &val, 1);
	if (ret) {
		LOG_ERR("Call `ds28e17_i2c_write_read` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(2000));
	LOG_DBG("BBB");

	m_w1_shields_count++;

	return 0;
}

static int ctr_sat_w1_scan(void)
{
	int ret;
	int res;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	ret = ctr_w1_acquire(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_acquire` failed: %d", ret);
		res = ret;
		goto error;
	}

	m_w1_shields_count = 0;

	ret = ctr_w1_scan(&m_w1, m_dev_ds2484, ctr_sat_w1_scan_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Call `ctr_w1_scan` failed: %d", ret);
		res = ret;
		goto error;
	}

	res = 0;

error:
	ret = ctr_w1_release(&m_w1, m_dev_ds2484);
	if (ret) {
		LOG_ERR("Call `ctr_w1_release` failed: %d", ret);
		res = res ? res : ret;
	}

	return res;
}
