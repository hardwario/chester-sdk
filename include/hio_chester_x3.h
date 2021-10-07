#ifndef HIO_CHESTER_X3_H
#define HIO_CHESTER_X3_H

// Standard includes
#include <stdint.h>

enum hio_chester_x3_slot {
    HIO_CHESTER_X3_SLOT_A = 0,
    HIO_CHESTER_X3_SLOT_B = 1
};

enum hio_chester_x3_channel {
    HIO_CHESTER_X3_CHANNEL_1 = 0,
    HIO_CHESTER_X3_CHANNEL_2 = 1
};

int hio_chester_x3_init(enum hio_chester_x3_slot slot,
                        enum hio_chester_x3_channel channel);
int hio_chester_x3_measure(enum hio_chester_x3_slot slot,
                           enum hio_chester_x3_channel channel,
                           int32_t *result);

// TODO
/*

enum hio_drv_ads122c04_mode {
    HIO_DRV_ADS122C04_MODE_NORMAL = 0,
    HIO_DRV_ADS122C04_MODE_TURBO = 1,
    HIO_DRV_ADS122C04_MODE_POWER_DOWN = 2
};

enum hio_drv_ads122c04_ref {
    HIO_DRV_ADS122C04_REF_INTERNAL = 0,
    HIO_DRV_ADS122C04_REF_EXTERNAL = 1
};

enum hio_drv_ads122c04_gain {
    HIO_DRV_ADS122C04_GAIN_1 = 0,
    HIO_DRV_ADS122C04_GAIN_2 = 0,
    HIO_DRV_ADS122C04_GAIN_4 = 0,
    HIO_DRV_ADS122C04_GAIN_8 = 0,
    HIO_DRV_ADS122C04_GAIN_16 = 0,
    HIO_DRV_ADS122C04_GAIN_32 = 0,
    HIO_DRV_ADS122C04_GAIN_64 = 0,
    HIO_DRV_ADS122C04_GAIN_128 = 0,
};

enum hio_drv_ads122c04_datarate {
    HIO_DRV_ADS122C04_DATARATE_NORMAL_20SPS = 0,
    HIO_DRV_ADS122C04_DATARATE_NORMAL_45SPS = 1,
    HIO_DRV_ADS122C04_DATARATE_NORMAL_90SPS = 2,
    HIO_DRV_ADS122C04_DATARATE_NORMAL_175SPS = 3,
    HIO_DRV_ADS122C04_DATARATE_NORMAL_330SPS = 4,
    HIO_DRV_ADS122C04_DATARATE_NORMAL_600SPS = 5,
    HIO_DRV_ADS122C04_DATARATE_NORMAL_1000SPS = 6,
    HIO_DRV_ADS122C04_DATARATE_TURBO_40SPS = 0,
    HIO_DRV_ADS122C04_DATARATE_TURBO_90SPS = 1,
    HIO_DRV_ADS122C04_DATARATE_TURBO_180SPS = 2,
    HIO_DRV_ADS122C04_DATARATE_TURBO_350SPS = 3,
    HIO_DRV_ADS122C04_DATARATE_TURBO_660SPS = 4,
    HIO_DRV_ADS122C04_DATARATE_TURBO_1200SPS = 5,
    HIO_DRV_ADS122C04_DATARATE_TURBO_2000SPS = 6,
};

enum hio_drv_ads122c04_channel {
    HIO_DRV_ADS122C04_CHANNEL_TODO = 0,
};


struct hio_drv_ads122c04_cfg {
    bool is_pga_enabled;
    enum hio_drv_ads122c04_ref ref;
};

*/

#endif
