#ifndef HIO_ADC_H
#define HIO_ADC_H

// Standard includes
#include <stdint.h>

enum hio_adc_channel {
    HIO_ADC_CHANNEL_A0 = 0,
    HIO_ADC_CHANNEL_A1 = 1,
    HIO_ADC_CHANNEL_A2 = 2,
    HIO_ADC_CHANNEL_A3 = 3,
    HIO_ADC_CHANNEL_B0 = 4,
    HIO_ADC_CHANNEL_B1 = 5,
    HIO_ADC_CHANNEL_B2 = 6,
    HIO_ADC_CHANNEL_B3 = 7
};

int hio_adc_init(enum hio_adc_channel channel);

int hio_adc_get_raw(enum hio_adc_channel_t channel,
                    uint16_t *raw);

int hio_adc_get_millivolts(enum hio_adc_channel_t channel,
                           uint16_t *millivolts);

#endif
