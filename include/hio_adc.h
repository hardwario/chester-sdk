#ifndef CHESTER_INCLUDE_ADC_H_
#define CHESTER_INCLUDE_ADC_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIO_ADC_MILLIVOLTS(_sample) (((uint32_t)(_sample)) * 600 * 6 / 4095)

enum hio_adc_channel {
	HIO_ADC_CHANNEL_A0 = 1,
	HIO_ADC_CHANNEL_A1 = 5,
	HIO_ADC_CHANNEL_A2 = 0,
	HIO_ADC_CHANNEL_A3 = 7,
	HIO_ADC_CHANNEL_B0 = 4,
	HIO_ADC_CHANNEL_B1 = 6,
	HIO_ADC_CHANNEL_B2 = 2,
	HIO_ADC_CHANNEL_B3 = 3
};

int hio_adc_init(enum hio_adc_channel channel);
int hio_adc_read(enum hio_adc_channel channel, uint16_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_ADC_H_ */
