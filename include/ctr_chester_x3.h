#ifndef CHESTER_INCLUDE_CHESTER_X3_H_
#define CHESTER_INCLUDE_CHESTER_X3_H_

/* Standard includes */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ctr_chester_x3_slot {
	CTR_CHESTER_X3_SLOT_A = 0,
	CTR_CHESTER_X3_SLOT_B = 1,
};

enum ctr_chester_x3_channel {
	CTR_CHESTER_X3_CHANNEL_1 = 0,
	CTR_CHESTER_X3_CHANNEL_2 = 1,
};

int ctr_chester_x3_init(enum ctr_chester_x3_slot slot, enum ctr_chester_x3_channel channel);
int ctr_chester_x3_measure(enum ctr_chester_x3_slot slot, enum ctr_chester_x3_channel channel,
                           int32_t *result);

/* TODO */
/*

enum ctr_drv_ads122c04_mode {
    CTR_DRV_ADS122C04_MODE_NORMAL = 0,
    CTR_DRV_ADS122C04_MODE_TURBO = 1,
    CTR_DRV_ADS122C04_MODE_POWER_DOWN = 2
};

enum ctr_drv_ads122c04_ref {
    CTR_DRV_ADS122C04_REF_INTERNAL = 0,
    CTR_DRV_ADS122C04_REF_EXTERNAL = 1
};

enum ctr_drv_ads122c04_gain {
    CTR_DRV_ADS122C04_GAIN_1 = 0,
    CTR_DRV_ADS122C04_GAIN_2 = 0,
    CTR_DRV_ADS122C04_GAIN_4 = 0,
    CTR_DRV_ADS122C04_GAIN_8 = 0,
    CTR_DRV_ADS122C04_GAIN_16 = 0,
    CTR_DRV_ADS122C04_GAIN_32 = 0,
    CTR_DRV_ADS122C04_GAIN_64 = 0,
    CTR_DRV_ADS122C04_GAIN_128 = 0,
};

enum ctr_drv_ads122c04_datarate {
    CTR_DRV_ADS122C04_DATARATE_NORMAL_20SPS = 0,
    CTR_DRV_ADS122C04_DATARATE_NORMAL_45SPS = 1,
    CTR_DRV_ADS122C04_DATARATE_NORMAL_90SPS = 2,
    CTR_DRV_ADS122C04_DATARATE_NORMAL_175SPS = 3,
    CTR_DRV_ADS122C04_DATARATE_NORMAL_330SPS = 4,
    CTR_DRV_ADS122C04_DATARATE_NORMAL_600SPS = 5,
    CTR_DRV_ADS122C04_DATARATE_NORMAL_1000SPS = 6,
    CTR_DRV_ADS122C04_DATARATE_TURBO_40SPS = 0,
    CTR_DRV_ADS122C04_DATARATE_TURBO_90SPS = 1,
    CTR_DRV_ADS122C04_DATARATE_TURBO_180SPS = 2,
    CTR_DRV_ADS122C04_DATARATE_TURBO_350SPS = 3,
    CTR_DRV_ADS122C04_DATARATE_TURBO_660SPS = 4,
    CTR_DRV_ADS122C04_DATARATE_TURBO_1200SPS = 5,
    CTR_DRV_ADS122C04_DATARATE_TURBO_2000SPS = 6,
};

enum ctr_drv_ads122c04_channel {
    CTR_DRV_ADS122C04_CHANNEL_TODO = 0,
};


struct ctr_drv_ads122c04_cfg {
    bool is_pga_enabled;
    enum ctr_drv_ads122c04_ref ref;
};

*/

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CHESTER_X3_H_ */
