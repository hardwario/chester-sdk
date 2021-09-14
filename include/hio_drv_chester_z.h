#ifndef HIO_DRV_CHESTER_Z
#define HIO_DRV_CHESTER_Z

#include <stdint.h>

enum chester_z_event {
    CHESTER_Z_EVENT_DC_CONNECTED = 0,
    CHESTER_Z_EVENT_DC_DISCONNECTED = 1
};

typedef void (*hio_drv_chester_z_event_cb)
    (enum chester_z_event event, void *param);

int hio_drv_chester_z_init(hio_drv_chester_z_event_cb cb, void *param);
int hio_drv_chester_z_get_vdc(uint16_t *millivolts);
int hio_drv_chester_z_get_vbat(uint16_t *millivolts);

#endif
