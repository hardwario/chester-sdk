#ifndef HIO_HYGRO_H
#define HIO_HYGRO_H

#ifdef __cplusplus
extern "C" {
#endif

int hio_hygro_init(void);
int hio_hygro_read(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif
