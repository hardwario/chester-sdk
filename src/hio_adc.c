#include <hio_adc.h>

// Zephyr includes
#include <device.h>
#include <devicetree.h>
#include <drivers/adc.h>
#include <logging/log.h>
#include <zephyr.h>

// Standard includes
#include <stddef.h>

LOG_MODULE_REGISTER(hio_adc, LOG_LEVEL_DBG);

#define ADC_DEVICE_NAME DT_LABEL(DT_NODELABEL(adc))

static const struct device *m_dev;

int hio_adc_init(enum hio_adc_channel channel)
{
    int ret;

    if (m_dev == NULL) {
        m_dev = device_get_binding(ADC_DEVICE_NAME);

        if (m_dev == NULL) {
            LOG_ERR("Call `device_get_binding` failed");
            return -ENODEV;
        }
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN_1_6,
        .reference = ADC_REF_INTERNAL,
        .acquisition_time = ADC_ACQ_TIME_DEFAULT,
        .channel_id = (uint8_t)channel,
        .differential = 0,
#if defined(CONFIG_ADC_CONFIGURABLE_INPUTS)
        .input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 +
                          (uint8_t)channel
#endif
    };

    ret = adc_channel_setup(m_dev, &channel_cfg);

    if (ret < 0) {
        LOG_ERR("Call `adc_channel_setup` failed: %d", ret);
        return ret;
    }

    return 0;
}

int hio_adc_read(enum hio_adc_channel channel, uint16_t *sample)
{
    int ret;

    struct adc_sequence sequence = {
        .options = NULL,
        .channels = BIT((uint8_t)channel),
        .buffer = sample,
        .buffer_size = sizeof(*sample),
        .resolution = 12,
        .oversampling = 4,
        .calibrate = true
    };

    ret = adc_read(m_dev, &sequence);

    if (ret < 0) {
        LOG_ERR("Call `adc_read` failed: %d", ret);
        return ret;
    }

    *sample = (int16_t)*sample < 0 ? 0 : *sample;

    return 0;
}
