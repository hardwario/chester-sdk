#include <ctr_bsp.h>
#include <ctr_bsp_i2c.h>
#include <ctr_drv_sht30.h>
#include <ctr_drv_tmp112.h>
#include <ctr_rtc.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/spi.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(ctr_bsp, LOG_LEVEL_DBG);

#define DEV_LED_R m_dev_gpio_1
#define PIN_LED_R 13

#define DEV_LED_G m_dev_gpio_1
#define PIN_LED_G 11

#define DEV_LED_Y m_dev_gpio_1
#define PIN_LED_Y 12

#define DEV_LED_EXT m_dev_gpio_1
#define PIN_LED_EXT 4

#define DEV_BTN_INT m_dev_gpio_0
#define PIN_BTN_INT 27

#define DEV_BTN_EXT m_dev_gpio_0
#define PIN_BTN_EXT 26

#define DEV_BAT_LOAD m_dev_gpio_1
#define PIN_BAT_LOAD 14

#define DEV_BAT_TEST m_dev_gpio_1
#define PIN_BAT_TEST 15

#define DEV_SLPZ m_dev_gpio_1
#define PIN_SLPZ 10

#define DEV_GNSS_ON m_dev_gpio_1
#define PIN_GNSS_ON 1

#define DEV_GNSS_RTC m_dev_gpio_1
#define PIN_GNSS_RTC 3

static const struct device *m_dev_gpio_0;
static const struct device *m_dev_gpio_1;
static const struct device *m_dev_i2c_0;
static const struct device *m_dev_spi_1;

static struct ctr_bus_i2c m_i2c;

static struct ctr_drv_sht30 m_sht30;
static struct ctr_drv_tmp112 m_tmp112;

static int init_gpio(void)
{
	m_dev_gpio_0 = device_get_binding("GPIO_0");

	if (m_dev_gpio_0 == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		return -ENODEV;
	}

	m_dev_gpio_1 = device_get_binding("GPIO_1");

	if (m_dev_gpio_1 == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		return -ENODEV;
	}

	return 0;
}

static int init_button(void)
{
	int ret;

	ret = gpio_pin_configure(DEV_BTN_INT, PIN_BTN_INT, GPIO_INPUT | GPIO_PULL_UP);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure(DEV_BTN_EXT, PIN_BTN_EXT, GPIO_INPUT | GPIO_PULL_UP);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init_i2c(void)
{
	int ret;

	m_dev_i2c_0 = device_get_binding("I2C_0");

	if (m_dev_i2c_0 == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		return -ENODEV;
	}

	const struct ctr_bus_i2c_driver *i2c_drv = ctr_bsp_i2c_get_driver();

	ret = ctr_bus_i2c_init(&m_i2c, i2c_drv, (void *)m_dev_i2c_0);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bus_i2c_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_drv_sht30_init(&m_sht30, &m_i2c, 0x45);

	if (ret < 0) {
		LOG_ERR("Call `ctr_drv_sht30_init` failed: %d", ret);
		return ret;
	}

	ret = ctr_drv_tmp112_init(&m_tmp112, &m_i2c, 0x48);

	if (ret < 0) {
		LOG_ERR("Call `ctr_drv_tmp112_init` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init_tmp112(void)
{
	int ret;

	ret = ctr_drv_tmp112_sleep(&m_tmp112);

	if (ret < 0) {
		LOG_ERR("Call `ctr_drv_tmp112_sleep` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init_flash(void)
{
	int ret;

	m_dev_spi_1 = device_get_binding("SPI_1");

	if (m_dev_spi_1 == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		return -ENODEV;
	}

	struct spi_cs_control spi_cs = { 0 };

	spi_cs.gpio_dev = m_dev_gpio_0;
	spi_cs.gpio_pin = 23;
	spi_cs.gpio_dt_flags = GPIO_ACTIVE_LOW;
	spi_cs.delay = 2;

	struct spi_config spi_cfg = { 0 };

	spi_cfg.frequency = 500000;
	spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE;
	spi_cfg.cs = &spi_cs;

	uint8_t buffer_tx[] = { 0x79 };

	const struct spi_buf tx_buf[] = { { .buf = buffer_tx, .len = sizeof(buffer_tx) } };

	const struct spi_buf_set tx = { .buffers = tx_buf, .count = 1 };

	const struct spi_buf rx_buf[] = { { .buf = NULL, .len = 1 } };

	const struct spi_buf_set rx = { .buffers = rx_buf, .count = 1 };

	ret = spi_transceive(m_dev_spi_1, &spi_cfg, &tx, &rx);

	if (ret < 0) {
		LOG_ERR("Call `spi_transceive` failed");
		return ret;
	}

	return 0;
}

int init_gnss(void)
{
	int ret;

	ret = gpio_pin_configure(DEV_GNSS_ON, PIN_GNSS_ON, GPIO_OUTPUT_INACTIVE);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure(DEV_GNSS_RTC, PIN_GNSS_RTC, GPIO_OUTPUT_INACTIVE);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

int init_batt(void)
{
	int ret;

	ret = gpio_pin_configure(DEV_BAT_LOAD, PIN_BAT_LOAD, GPIO_OUTPUT_INACTIVE);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_configure(DEV_BAT_TEST, PIN_BAT_TEST, GPIO_OUTPUT_INACTIVE);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

int init_w1b(void)
{
	int ret;

	ret = gpio_pin_configure(DEV_SLPZ, PIN_SLPZ, GPIO_OUTPUT_INACTIVE);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

struct ctr_bus_i2c *ctr_bsp_get_i2c(void)
{
	return &m_i2c;
}

int ctr_bsp_set_led(enum ctr_bsp_led led, bool on)
{
	int ret;

	switch (led) {
	case CTR_BSP_LED_R:
		ret = gpio_pin_set(DEV_LED_R, PIN_LED_R, on ? 1 : 0);

		if (ret < 0) {
			LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
			return ret;
		}

		break;

	case CTR_BSP_LED_G:
		ret = gpio_pin_set(DEV_LED_G, PIN_LED_G, on ? 1 : 0);

		if (ret < 0) {
			LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
			return ret;
		}

		break;

	case CTR_BSP_LED_Y:
		ret = gpio_pin_set(DEV_LED_Y, PIN_LED_Y, on ? 1 : 0);

		if (ret < 0) {
			LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
			return ret;
		}

		break;

	case CTR_BSP_LED_EXT:
		ret = gpio_pin_set(DEV_LED_EXT, PIN_LED_EXT, on ? 1 : 0);

		if (ret < 0) {
			LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
			return ret;
		}

		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int ctr_bsp_get_button(enum ctr_bsp_button button, bool *pressed)
{
	int ret;

	switch (button) {
	case CTR_BSP_BUTTON_INT:
		ret = gpio_pin_get(DEV_BTN_INT, PIN_BTN_INT);

		if (ret < 0) {
			LOG_ERR("Call `gpio_pin_get` failed: %d", ret);
			return ret;
		}

		*pressed = ret == 0 ? true : false;
		break;

	case CTR_BSP_BUTTON_EXT:

		ret = gpio_pin_get(DEV_BTN_EXT, PIN_BTN_EXT);

		if (ret < 0) {
			LOG_ERR("Call `gpio_pin_get` failed: %d", ret);
			return ret;
		}

		*pressed = ret == 0 ? true : false;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

int ctr_bsp_set_batt_load(bool on)
{
	int ret;

	ret = gpio_pin_set(DEV_BAT_LOAD, PIN_BAT_LOAD, on ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_bsp_set_batt_test(bool on)
{
	int ret;

	ret = gpio_pin_set(DEV_BAT_TEST, PIN_BAT_TEST, on ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_bsp_set_gnss_on(bool on)
{
	int ret;

	ret = gpio_pin_set(DEV_GNSS_ON, PIN_GNSS_ON, on ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_bsp_set_gnss_rtc(bool on)
{
	int ret;

	ret = gpio_pin_set(DEV_GNSS_RTC, PIN_GNSS_RTC, on ? 1 : 0);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_bsp_set_w1b_slpz(int level)
{
	int ret;

	ret = gpio_pin_set(DEV_SLPZ, PIN_SLPZ, level == 0 ? 0 : 1);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_set` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_bsp_sht30_measure(float *t, float *rh)
{
	int ret;

	ret = ctr_drv_sht30_measure(&m_sht30, t, rh);

	if (ret < 0) {
		LOG_ERR("Call `ctr_drv_sht30_measure` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_bsp_tmp112_measure(float *t)
{
	int ret;

	ret = ctr_drv_tmp112_measure(&m_tmp112, t);

	if (ret < 0) {
		LOG_ERR("Call `ctr_bsp_tmp112_measure` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	ret = init_gpio();

	if (ret < 0) {
		LOG_ERR("Call `init_gpio` failed: %d", ret);
		return ret;
	}

	ret = init_button();

	if (ret < 0) {
		LOG_ERR("Call `init_button` failed: %d", ret);
		return ret;
	}

	ret = init_i2c();

	if (ret < 0) {
		LOG_ERR("Call `init_i2c` failed: %d", ret);
		return ret;
	}

	ret = init_tmp112();

	if (ret < 0) {
		LOG_ERR("Call `init_tmp112` failed: %d", ret);
		return ret;
	}

	ret = init_flash();

	if (ret < 0) {
		LOG_ERR("Call `init_flash` failed: %d", ret);
		return ret;
	}

	ret = init_gnss();

	if (ret < 0) {
		LOG_ERR("Call `init_gnss` failed: %d", ret);
		return ret;
	}

	ret = init_batt();

	if (ret < 0) {
		LOG_ERR("Call `init_batt` failed: %d", ret);
		return ret;
	}

	ret = init_w1b();

	if (ret < 0) {
		LOG_ERR("Call `init_w1b` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_BSP_INIT_PRIORITY);
