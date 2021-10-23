#ifndef CHESTER_INCLUDE_CHESTER_X0D_H_
#define CHESTER_INCLUDE_CHESTER_X0D_H_

#ifdef __cplusplus
extern "C" {
#endif

enum hio_chester_x0d_slot {
	HIO_CHESTER_X0D_SLOT_A = 0,
	HIO_CHESTER_X0D_SLOT_B = 1,
};

enum hio_chester_x0d_channel {
	HIO_CHESTER_X0D_CHANNEL_1 = 0,
	HIO_CHESTER_X0D_CHANNEL_2 = 1,
	HIO_CHESTER_X0D_CHANNEL_3 = 2,
	HIO_CHESTER_X0D_CHANNEL_4 = 3
};

enum hio_chester_x0d_event {
	HIO_CHESTER_X0D_EVENT_RISE = 0,
	HIO_CHESTER_X0D_EVENT_FALL = 1,
};

typedef void (*hio_chester_x0d_event_cb)(enum hio_chester_x0d_slot slot,
                                         enum hio_chester_x0d_channel channel,
                                         enum hio_chester_x0d_event event, void *param);

int hio_chester_x0d_init(enum hio_chester_x0d_slot slot, enum hio_chester_x0d_channel channel,
                         hio_chester_x0d_event_cb callback, void *param);
int hio_chester_x0d_read(enum hio_chester_x0d_slot slot, enum hio_chester_x0d_channel channel,
                         int *level);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CHESTER_X0D_H_ */
