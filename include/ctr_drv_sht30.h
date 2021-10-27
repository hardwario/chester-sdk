#ifndef CHESTER_INCLUDE_DRV_SHT30_H_
#define CHESTER_INCLUDE_DRV_SHT30_H_

#include <ctr_bus_i2c.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_drv_sht30 {
	struct ctr_bus_i2c *i2c;
	uint8_t dev_addr;
	bool ready;
};

int ctr_drv_sht30_init(struct ctr_drv_sht30 *ctx, struct ctr_bus_i2c *i2c, uint8_t dev_addr);
int ctr_drv_sht30_measure(struct ctr_drv_sht30 *ctx, float *t, float *rh);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRV_SHT30_H_ */
