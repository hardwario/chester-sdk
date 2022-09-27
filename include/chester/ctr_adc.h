#ifndef CHESTER_INCLUDE_CTR_ADC_H_
#define CHESTER_INCLUDE_CTR_ADC_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTR_ADC_MILLIVOLTS(_sample) (((uint32_t)(_sample)) * 600 * 6 / 4095)
#define CTR_ADC_X0_CL_MILLIAMPS(_sample)                                                           \
	((float)CTR_ADC_MILLIVOLTS(_sample) * ((100.f + 10.f) / 10.f) / 249.f)

enum ctr_adc_channel {
	CTR_ADC_CHANNEL_A0 = 1,
	CTR_ADC_CHANNEL_A1 = 5,
	CTR_ADC_CHANNEL_A2 = 0,
	CTR_ADC_CHANNEL_A3 = 7,
	CTR_ADC_CHANNEL_B0 = 4,
	CTR_ADC_CHANNEL_B1 = 6,
	CTR_ADC_CHANNEL_B2 = 2,
	CTR_ADC_CHANNEL_B3 = 3
};

int ctr_adc_init(enum ctr_adc_channel channel);
int ctr_adc_read(enum ctr_adc_channel channel, uint16_t *sample);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_ADC_H_ */
