#ifndef HIO_THERM_H
#define HIO_THERM_H

#ifdef __cplusplus
extern "C" {
#endif

int hio_therm_init(void);
int hio_therm_read(float *temperature);

#ifdef __cplusplus
}
#endif

#endif
