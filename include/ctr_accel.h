#ifndef CHESTER_INCLUDE_CTR_ACCEL_H_
#define CHESTER_INCLUDE_CTR_ACCEL_H_

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_accel_event {
	CTR_ACCEL_EVENT_TILT = 0,
};

typedef void (*ctr_accel_user_cb)(enum ctr_accel_event event, void *user_data);

int ctr_accel_set_event_handler(ctr_accel_user_cb user_cb, void *user_data);
int ctr_accel_set_threshold(float threshold, int duration);
int ctr_accel_enable_trigger(void);
int ctr_accel_read(float *accel_x, float *accel_y, float *accel_z, int *orientation);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_ACCEL_H_ */
