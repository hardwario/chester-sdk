
#include <device.h>
#include <drivers/adc.h>
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_adc, LOG_LEVEL_DBG);

static const struct device *m_dev;

int hio_adc_init(hio_adc_channel channel)
{
    int ret;

    m_dev = device_get_binding("ADC_0");

    if (m_dev == NULL) {
        LOG_ERR("Call `device_get_binding` failed");
        return -ENODEV;
    }

    struct adc_channel_cfg channel_cfg = {0};

    channel_cfg.acquisition_time = ADC_ACQ_TIME_DEFAULT;
    channel_cfg.channel_id = 0; // TODO Set channel
    channel_cfg.differential = 0;
    channel_cfg.reference = ADC_REF_INTERNAL;
    channel_cfg.gain = ADC_GAIN_1;

    ret = adc_channel_setup(m_dev, &channel_cfg);

    if (ret < 0) {
        LOG_ERR("Call `adc_channel_setup` failed: %d", ret);
        return -ENODEV;
    }

    return 0;
}
