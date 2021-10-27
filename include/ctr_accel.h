#ifndef CHESTER_INCLUDE_ACCEL_H_
#define CHESTER_INCLUDE_ACCEL_H_

#ifdef __cplusplus
extern "C" {
#endif

int ctr_accel_read(float *accel_x, float *accel_y, float *accel_z, int *orientation);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_ACCEL_H_ */
