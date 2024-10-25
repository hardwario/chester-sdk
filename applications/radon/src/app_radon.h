#ifndef APP_RADON_H_
#define APP_RADON_H_

#include <stdint.h>
#include <stdbool.h>

int app_radon_disable(void);
int app_radon_enable(void);
int app_radon_init(void);
int app_radon_read_temperature(int16_t *out);
int app_radon_read_humidity(uint16_t *out);
int app_radon_read_concentration(uint32_t *out, bool daily);

#endif /* !APP_RADON_H_ */