#include "hio_chester_z_reg.h"
#include <hio_chester_z.h>
#include <hio_bsp.h>
#include <hio_bus_i2c.h>

/* Zephyr includes */
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_chester_z, LOG_LEVEL_DBG);

#define INT_DEV DT_LABEL(DT_NODELABEL(gpio1))
#define INT_PIN 6

#define DEV_ADDR 0x4f

static struct hio_bus_i2c *m_i2c;
static hio_chester_z_event_cb m_cb;
static void *m_param;
static const struct device *m_int_dev;
static struct gpio_callback m_int_callback;
static uint16_t m_protocol;

static int read(uint8_t addr, uint16_t *data)
{
	int ret;

	ret = hio_bus_i2c_mem_read_16b(m_i2c, DEV_ADDR, addr, data);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_read_16b` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int write(uint8_t addr, uint16_t data)
{
	int ret;

	ret = hio_bus_i2c_mem_write_16b(m_i2c, DEV_ADDR, addr, data);

	if (ret < 0) {
		LOG_ERR("Call `hio_bus_i2c_mem_write_16b` failed: %d", ret);
		return ret;
	}

	return 0;
}

static void int_work_handler(struct k_work *work)
{
	int ret;
	uint16_t reg_irq0;

	ret = read(REG_IRQ0, &reg_irq0);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_oops();
	}

	uint16_t reg_irq1;

	ret = read(REG_IRQ1, &reg_irq1);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		k_oops();
	}

#define CALLBACK(_reg, _mask, _event)                                                              \
	do {                                                                                       \
		if (((_reg) & (_mask)) != 0) {                                                     \
			m_cb((_event), m_param);                                                   \
		}                                                                                  \
	} while (0)

	if (m_cb != NULL) {
		CALLBACK(reg_irq0, 0x0080, HIO_CHESTER_Z_EVENT_DC_CONNECTED);
		CALLBACK(reg_irq0, 0x0040, HIO_CHESTER_Z_EVENT_DC_DISCONNECTED);
		CALLBACK(reg_irq0, 0x0008, HIO_CHESTER_Z_EVENT_BUTTON_0_PRESS);
		CALLBACK(reg_irq0, 0x0004, HIO_CHESTER_Z_EVENT_BUTTON_0_CLICK);
		CALLBACK(reg_irq0, 0x0002, HIO_CHESTER_Z_EVENT_BUTTON_0_HOLD);
		CALLBACK(reg_irq0, 0x0001, HIO_CHESTER_Z_EVENT_BUTTON_0_RELEASE);
		CALLBACK(reg_irq1, 0x8000, HIO_CHESTER_Z_EVENT_BUTTON_1_PRESS);
		CALLBACK(reg_irq1, 0x4000, HIO_CHESTER_Z_EVENT_BUTTON_1_CLICK);
		CALLBACK(reg_irq1, 0x2000, HIO_CHESTER_Z_EVENT_BUTTON_1_HOLD);
		CALLBACK(reg_irq1, 0x1000, HIO_CHESTER_Z_EVENT_BUTTON_1_RELEASE);
		CALLBACK(reg_irq1, 0x0800, HIO_CHESTER_Z_EVENT_BUTTON_2_PRESS);
		CALLBACK(reg_irq1, 0x0400, HIO_CHESTER_Z_EVENT_BUTTON_2_CLICK);
		CALLBACK(reg_irq1, 0x0200, HIO_CHESTER_Z_EVENT_BUTTON_2_HOLD);
		CALLBACK(reg_irq1, 0x0100, HIO_CHESTER_Z_EVENT_BUTTON_2_RELEASE);
		CALLBACK(reg_irq1, 0x0080, HIO_CHESTER_Z_EVENT_BUTTON_3_PRESS);
		CALLBACK(reg_irq1, 0x0040, HIO_CHESTER_Z_EVENT_BUTTON_3_CLICK);
		CALLBACK(reg_irq1, 0x0020, HIO_CHESTER_Z_EVENT_BUTTON_3_HOLD);
		CALLBACK(reg_irq1, 0x0010, HIO_CHESTER_Z_EVENT_BUTTON_3_RELEASE);
		CALLBACK(reg_irq1, 0x0008, HIO_CHESTER_Z_EVENT_BUTTON_4_PRESS);
		CALLBACK(reg_irq1, 0x0004, HIO_CHESTER_Z_EVENT_BUTTON_4_CLICK);
		CALLBACK(reg_irq1, 0x0002, HIO_CHESTER_Z_EVENT_BUTTON_4_HOLD);
		CALLBACK(reg_irq1, 0x0001, HIO_CHESTER_Z_EVENT_BUTTON_4_RELEASE);
	}

#undef CALLBACK

	ret = write(REG_IRQ0, reg_irq0);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_oops();
	}

	ret = write(REG_IRQ1, reg_irq1);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		k_oops();
	}

	ret = gpio_pin_interrupt_configure(m_int_dev, INT_PIN, GPIO_INT_LEVEL_LOW);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
		k_oops();
	}
}

static K_WORK_DEFINE(m_int_work, int_work_handler);

static void gpio_callback_handler(const struct device *port, struct gpio_callback *cb,
                                  gpio_port_pins_t pins)
{
	int ret;

	ret = gpio_pin_interrupt_configure(m_int_dev, INT_PIN, GPIO_INT_DISABLE);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
		k_oops();
	}

	k_work_submit(&m_int_work);
}

int hio_chester_z_init(hio_chester_z_event_cb cb, void *param)
{
	int ret;

	m_i2c = hio_bsp_get_i2c();
	m_cb = cb;
	m_param = param;

	uint16_t reg_protocol;

	ret = read(REG_PROTOCOL, &reg_protocol);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	m_protocol = reg_protocol;

	ret = write(REG_CONTROL, 0x0003);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	ret = write(REG_IRQ0, 0xffff);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	ret = write(REG_IRQ1, 0xffff);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	m_int_dev = device_get_binding(INT_DEV);

	if (m_int_dev == NULL) {
		LOG_ERR("Call `device_get_binding` failed");
		return -ENODEV;
	}

	ret = gpio_pin_configure(m_int_dev, INT_PIN, GPIO_INPUT | GPIO_PULL_UP);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_configure` failed: %d", ret);
		return ret;
	}

	gpio_init_callback(&m_int_callback, gpio_callback_handler, BIT(INT_PIN));

	ret = gpio_add_callback(m_int_dev, &m_int_callback);

	if (ret < 0) {
		LOG_ERR("Call `gpio_add_callback` failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure(m_int_dev, INT_PIN, GPIO_INT_LEVEL_LOW);

	if (ret < 0) {
		LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_chester_z_apply(void)
{
	int ret;

	ret = write(REG_CONTROL, 0x8003);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_chester_z_get_status(struct hio_chester_z_status *status)
{
	int ret;

	uint16_t reg_status;

	ret = read(REG_STATUS, &reg_status);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	status->dc_input_connected = (reg_status & 0x0040) != 0 ? true : false;
	status->button_0_pressed = (reg_status & 0x0010) != 0 ? true : false;
	status->button_1_pressed = (reg_status & 0x0008) != 0 ? true : false;
	status->button_2_pressed = (reg_status & 0x0004) != 0 ? true : false;
	status->button_3_pressed = (reg_status & 0x0002) != 0 ? true : false;
	status->button_4_pressed = (reg_status & 0x0001) != 0 ? true : false;

	return 0;
}

int hio_chester_z_get_vdc_mv(uint16_t *vdc)
{
	int ret;

	uint16_t reg_vdc;

	ret = read(REG_VDC, &reg_vdc);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*vdc = reg_vdc;

	return 0;
}

int hio_chester_z_get_vbat_mv(uint16_t *vbat)
{
	int ret;

	uint16_t reg_vbat;

	ret = read(REG_VBAT, &reg_vbat);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*vbat = reg_vbat;

	return 0;
}

int hio_chester_z_get_serial_number(uint32_t *serial_number)
{
	int ret;

	uint16_t reg_serno0;

	ret = read(REG_SERNO0, &reg_serno0);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_serno1;

	ret = read(REG_SERNO1, &reg_serno1);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*serial_number = (uint32_t)reg_serno1 << 8 | (uint32_t)reg_serno0;

	return 0;
}

int hio_chester_z_get_hw_revision(uint16_t *hw_revision)
{
	int ret;

	uint16_t reg_hwrev;

	ret = read(REG_HWREV, &reg_hwrev);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*hw_revision = reg_hwrev;

	return 0;
}

int hio_chester_z_get_hw_variant(uint32_t *hw_variant)
{
	int ret;

	uint16_t reg_hwvar0;

	ret = read(REG_HWVAR0, &reg_hwvar0);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_hwvar1;

	ret = read(REG_HWVAR1, &reg_hwvar1);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*hw_variant = (uint32_t)reg_hwvar1 << 8 | (uint32_t)reg_hwvar0;

	return 0;
}

int hio_chester_z_get_fw_version(uint32_t *fw_version)
{
	int ret;

	uint16_t reg_fwver0;

	ret = read(REG_FWVER0, &reg_fwver0);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	uint16_t reg_fwver1;

	ret = read(REG_FWVER1, &reg_fwver1);

	if (ret < 0) {
		LOG_ERR("Call `read` failed: %d", ret);
		return ret;
	}

	*fw_version = (uint32_t)reg_fwver1 << 8 | (uint32_t)reg_fwver0;

	return 0;
}

int hio_chester_z_get_vendor_name(char **vendor_name)
{
	int ret;

	static char buf[32];

	for (size_t i = 0; i < ARRAY_SIZE(buf); i++) {
		uint16_t reg_vend;

		ret = read(REG_VEND0 + (uint8_t)i, &reg_vend);

		if (ret < 0) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		buf[i] = (char)reg_vend;
	}

	*vendor_name = buf;

	return 0;
}

int hio_chester_z_get_product_name(char **product_name)
{
	int ret;

	static char buf[32];

	for (size_t i = 0; i < ARRAY_SIZE(buf); i++) {
		uint16_t reg_prod;

		ret = read(REG_PROD0 + (uint8_t)i, &reg_prod);

		if (ret < 0) {
			LOG_ERR("Call `read` failed: %d", ret);
			return ret;
		}

		buf[i] = (char)reg_prod;
	}

	*product_name = buf;

	return 0;
}

int hio_chester_z_set_buzzer(const struct hio_chester_z_buzzer_param *param)
{
	int ret;

	uint16_t reg_buzzer;

	reg_buzzer = (uint16_t)param->command << 4 | (uint16_t)param->pattern;

	ret = write(REG_BUZZER, reg_buzzer);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}

int hio_chester_z_set_led(enum hio_chester_z_led_channel channel,
                          const struct hio_chester_z_led_param *param)
{
	int ret;

	uint16_t reg_led;

	reg_led = (uint16_t)param->brightness << 8 | (uint16_t)param->command << 4 |
	          (uint16_t)param->pattern;

	ret = write(REG_LED0R + (uint8_t)channel, reg_led);

	if (ret < 0) {
		LOG_ERR("Call `write` failed: %d", ret);
		return ret;
	}

	return 0;
}
