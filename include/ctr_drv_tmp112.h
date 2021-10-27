#ifndef CHESTER_INCLUDE_DRV_TMP112_H_
#define CHESTER_INCLUDE_DRV_TMP112_H_

#include <ctr_bus_i2c.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_drv_tmp112 {
	struct ctr_bus_i2c *i2c;
	uint8_t dev_addr;
	bool ready;
};

int ctr_drv_tmp112_init(struct ctr_drv_tmp112 *ctx, struct ctr_bus_i2c *i2c, uint8_t dev_addr);
int ctr_drv_tmp112_measure(struct ctr_drv_tmp112 *ctx, float *t);
int ctr_drv_tmp112_sleep(struct ctr_drv_tmp112 *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRV_TMP112_H_ */
