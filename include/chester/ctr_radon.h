#ifndef CTR_RADON_H_
#define CTR_RADON_H_

int ctr_radon_disable(void);
int ctr_radon_enable(void);
int ctr_radon_read_temperature(int16_t *out);
int ctr_radon_read_humidity(uint16_t *out);
int ctr_radon_read_concentration(uint32_t *out, bool daily);

#endif /* !CTR_RADON_H_ */