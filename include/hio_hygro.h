#ifndef HIO_HYGRO_H
#define HIO_HYGRO_H

int hio_hygro_init(void);
int hio_hygro_read(float *temperature, float *humidity);

#endif
