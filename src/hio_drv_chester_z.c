#include <hio_drv_chester_z.h>
#include <hio_bsp.h>
#include <hio_bus_i2c.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_drv_chester_z, LOG_LEVEL_DBG);

#define DEV_ADDR 0x4f
#define INT_PIN 6

#define REG_PROTOCOL 0x00
#define REG_CONTROL 0x01
#define REG_BUZZER 0x02
#define REG_LED0R 0x03
#define REG_LED0G 0x04
#define REG_LED0B 0x05
#define REG_LED1R 0x06
#define REG_LED1G 0x07
#define REG_LED1B 0x08
#define REG_LED2R 0x09
#define REG_LED2G 0x0a
#define REG_LED2B 0x0b
#define REG_LED3R 0x0c
#define REG_LED3G 0x0d
#define REG_LED3B 0x0e
#define REG_LED4R 0x0f
#define REG_LED4G 0x10
#define REG_LED4B 0x11
#define REG_STATUS 0x12
#define REG_VDC 0x13
#define REG_VBAT 0x14
#define REG_IRQ0 0x15
#define REG_IRQ1 0x16
#define REG_SERNO0 0xb9
#define REG_SERNO1 0xba
#define REG_HWREV 0xbb
#define REG_HWVAR0 0xbc
#define REG_HWVAR1 0xbd
#define REG_FWVER0 0xbe
#define REG_FWVER1 0xbf
#define REG_VEND0 0xc0
#define REG_VEND1 0xc1
#define REG_VEND2 0xc2
#define REG_VEND3 0xc3
#define REG_VEND4 0xc4
#define REG_VEND5 0xc5
#define REG_VEND6 0xc6
#define REG_VEND7 0xc7
#define REG_VEND8 0xc8
#define REG_VEND9 0xc9
#define REG_VEND10 0xca
#define REG_VEND11 0xcb
#define REG_VEND12 0xcc
#define REG_VEND13 0xcd
#define REG_VEND14 0xce
#define REG_VEND15 0xcf
#define REG_VEND16 0xd0
#define REG_VEND17 0xd1
#define REG_VEND18 0xd2
#define REG_VEND19 0xd3
#define REG_VEND20 0xd4
#define REG_VEND21 0xd5
#define REG_VEND22 0xd6
#define REG_VEND23 0xd7
#define REG_VEND24 0xd8
#define REG_VEND25 0xd9
#define REG_VEND26 0xda
#define REG_VEND27 0xdb
#define REG_VEND28 0xdc
#define REG_VEND29 0xdd
#define REG_VEND30 0xde
#define REG_VEND31 0xdf
#define REG_PROD0 0xe0
#define REG_PROD1 0xe1
#define REG_PROD2 0xe2
#define REG_PROD3 0xe3
#define REG_PROD4 0xe4
#define REG_PROD5 0xe5
#define REG_PROD6 0xe6
#define REG_PROD7 0xe7
#define REG_PROD8 0xe8
#define REG_PROD9 0xe9
#define REG_PROD10 0xea
#define REG_PROD11 0xeb
#define REG_PROD12 0xec
#define REG_PROD13 0xed
#define REG_PROD14 0xee
#define REG_PROD15 0xef
#define REG_PROD16 0xf0
#define REG_PROD17 0xf1
#define REG_PROD18 0xf2
#define REG_PROD19 0xf3
#define REG_PROD20 0xf4
#define REG_PROD21 0xf5
#define REG_PROD22 0xf6
#define REG_PROD23 0xf7
#define REG_PROD24 0xf8
#define REG_PROD25 0xf9
#define REG_PROD26 0xfa
#define REG_PROD27 0xfb
#define REG_PROD28 0xfc
#define REG_PROD29 0xfd
#define REG_PROD30 0xfe
#define REG_PROD31 0xff

static struct hio_bus_i2c *m_i2c;
static hio_drv_chester_z_event_cb m_cb;
static void *m_param;
static const struct device *m_int_dev;
static struct gpio_callback m_int_callback;

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

    if (m_cb != NULL) {
        if ((reg_irq0 & 0x0080) != 0) {
            m_cb(CHESTER_Z_EVENT_DC_CONNECTED, m_param);
        }

        if ((reg_irq0 & 0x0040) != 0) {
            m_cb(CHESTER_Z_EVENT_DC_DISCONNECTED, m_param);
        }

        // TODO Process all events
    }

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

static void gpio_callback_handler(const struct device *port,
                                  struct gpio_callback *cb,
                                  gpio_port_pins_t pins)
{
    int ret;

    LOG_DBG("Event %u", k_is_in_isr());

    ret = gpio_pin_interrupt_configure(m_int_dev, INT_PIN, GPIO_INT_DISABLE);

    if (ret < 0) {
        LOG_ERR("Call `gpio_pin_interrupt_configure` failed: %d", ret);
    }

    k_work_submit(&m_int_work);
}

int hio_drv_chester_z_init(hio_drv_chester_z_event_cb cb, void *param)
{
    int ret;

    m_i2c = hio_bsp_get_i2c();
    m_cb = cb;
    m_param = param;

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

    m_int_dev = device_get_binding("GPIO_1");

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

int hio_drv_chester_z_get_vdc(uint16_t *millivolts)
{
    return read(REG_VDC, millivolts);
}

int hio_drv_chester_z_get_vbat(uint16_t *millivolts)
{
    return read(REG_VBAT, millivolts);
}
