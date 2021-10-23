#ifndef HIO_ACCEL_H
#define HIO_ACCEL_H

#ifdef __cplusplus
extern "C" {
#endif

int hio_accel_read(float *accel_x, float *accel_y, float *accel_z, int *orientation);

#ifdef __cplusplus
}
#endif

#endif
