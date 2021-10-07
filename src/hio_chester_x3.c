#include <hio_chester_x3.h>
#include <hio_bus_i2c.h>
#include <hio_bsp.h>
#include <hio_drv_ads122c04.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_chester_x3, LOG_LEVEL_DBG);

static struct hio_drv_ads122c04 m_ads122c04_a_ch_1;
static struct hio_drv_ads122c04 m_ads122c04_a_ch_2;
static struct hio_drv_ads122c04 m_ads122c04_b_ch_1;
static struct hio_drv_ads122c04 m_ads122c04_b_ch_2;

int hio_chester_x3_init(enum hio_chester_x3_slot slot,
                        enum hio_chester_x3_channel channel)
{
    int ret;

    struct hio_drv_ads122c04 *ads122c04;
    uint8_t dev_addr;

    if (slot == HIO_CHESTER_X3_SLOT_A &&
        channel == HIO_CHESTER_X3_CHANNEL_1) {

        ads122c04 = &m_ads122c04_a_ch_1;
        dev_addr = 0x40; // TODO Change for new CHESTER-X3

    } else if (slot == HIO_CHESTER_X3_SLOT_A &&
               channel == HIO_CHESTER_X3_CHANNEL_2) {

        ads122c04 = &m_ads122c04_a_ch_2;
        dev_addr = 0x41; // TODO Change for new CHESTER-X3

    } else if (slot == HIO_CHESTER_X3_SLOT_B &&
               channel == HIO_CHESTER_X3_CHANNEL_1) {

        ads122c04 = &m_ads122c04_b_ch_1;
        dev_addr = 0x40; // TODO Change for new CHESTER-X3

    } else if (slot == HIO_CHESTER_X3_SLOT_B &&
               channel == HIO_CHESTER_X3_CHANNEL_2) {

        ads122c04 = &m_ads122c04_b_ch_2;
        dev_addr = 0x41; // TODO Change for new CHESTER-X3

    } else {
        return -EINVAL;
    }

    ret = hio_drv_ads122c04_init(ads122c04, hio_bsp_get_i2c(), dev_addr);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_init` failed: %d", ret);
        return ret;
    }

    ret = hio_drv_ads122c04_reset(ads122c04);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_reset` failed: %d", ret);
        return ret;
    }

    ret = hio_drv_ads122c04_write_reg(ads122c04, 0x00, 0x30);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_write_reg` failed: %d", ret);
        return ret;
    }

    ret = hio_drv_ads122c04_write_reg(ads122c04, 0x01, 0x02);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_write_reg` failed: %d", ret);
        return ret;
    }

    ret = hio_drv_ads122c04_write_reg(ads122c04, 0x02, 0x06);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_write_reg` failed: %d", ret);
        return ret;
    }

    ret = hio_drv_ads122c04_write_reg(ads122c04, 0x03, 0x80);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_write_reg` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_chester_x3_measure(enum hio_chester_x3_slot slot,
                           enum hio_chester_x3_channel channel,
                           int32_t *result)
{
    int ret;

    struct hio_drv_ads122c04 *ads122c04;

    if (slot == HIO_CHESTER_X3_SLOT_A &&
        channel == HIO_CHESTER_X3_CHANNEL_1) {

        ads122c04 = &m_ads122c04_a_ch_1;

    } else if (slot == HIO_CHESTER_X3_SLOT_A &&
               channel == HIO_CHESTER_X3_CHANNEL_2) {

        ads122c04 = &m_ads122c04_a_ch_2;

    } else if (slot == HIO_CHESTER_X3_SLOT_B &&
               channel == HIO_CHESTER_X3_CHANNEL_1) {

        ads122c04 = &m_ads122c04_b_ch_1;

    } else if (slot == HIO_CHESTER_X3_SLOT_B &&
               channel == HIO_CHESTER_X3_CHANNEL_2) {

        ads122c04 = &m_ads122c04_b_ch_2;

    } else {
        return -EINVAL;
    }

    ret = hio_drv_ads122c04_start_sync(ads122c04);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_start_sync` failed: %d", ret);
        return ret;
    }

    int retries = 30;

    while (--retries > 0) {
        uint8_t reg;

        ret = hio_drv_ads122c04_read_reg(ads122c04, 0x02, &reg);

        if ((reg & 0x80) != 0) {
            break;
        }

        k_sleep(K_MSEC(10));
    }

    if (retries == 0) {
        ret = hio_drv_ads122c04_powerdown(ads122c04);

        if (ret < 0) {
            LOG_ERR("Call `hio_drv_ads122c04_powerdown` failed: %d", ret);
            return ret;
        }

        return -EIO;
    }

    ret = hio_drv_ads122c04_read_data(ads122c04, result);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_read_data` failed: %d", ret);
        return ret;
    }

    LOG_DBG("Result: %" PRId32, *result);

    ret = hio_drv_ads122c04_powerdown(ads122c04);

    if (ret < 0) {
        LOG_ERR("Call `hio_drv_ads122c04_powerdown` failed: %d", ret);
        return ret;
    }

    return 0;
}
