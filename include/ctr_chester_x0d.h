#ifndef CHESTER_INCLUDE_CTR_CHESTER_X0D_H_
#define CHESTER_INCLUDE_CTR_CHESTER_X0D_H_

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_chester_x0d_slot {
	CTR_CHESTER_X0D_SLOT_A = 0,
	CTR_CHESTER_X0D_SLOT_B = 1,
};

enum ctr_chester_x0d_channel {
	CTR_CHESTER_X0D_CHANNEL_1 = 0,
	CTR_CHESTER_X0D_CHANNEL_2 = 1,
	CTR_CHESTER_X0D_CHANNEL_3 = 2,
	CTR_CHESTER_X0D_CHANNEL_4 = 3
};

enum ctr_chester_x0d_event {
	CTR_CHESTER_X0D_EVENT_RISE = 0,
	CTR_CHESTER_X0D_EVENT_FALL = 1,
};

typedef void (*ctr_chester_x0d_event_cb)(enum ctr_chester_x0d_slot slot,
                                         enum ctr_chester_x0d_channel channel,
                                         enum ctr_chester_x0d_event event, void *param);

int ctr_chester_x0d_init(enum ctr_chester_x0d_slot slot, enum ctr_chester_x0d_channel channel,
                         ctr_chester_x0d_event_cb callback, void *param);
int ctr_chester_x0d_read(enum ctr_chester_x0d_slot slot, enum ctr_chester_x0d_channel channel,
                         int *level);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CHESTER_X0D_H_ */
