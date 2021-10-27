#ifndef CHESTER_INCLUDE_DRV_ADS122C04_H_
#define CHESTER_INCLUDE_DRV_ADS122C04_H_

#include <ctr_bus_i2c.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_drv_ads122c04 {
	struct ctr_bus_i2c *i2c;
	uint8_t dev_addr;
};

int ctr_drv_ads122c04_init(struct ctr_drv_ads122c04 *ctx, struct ctr_bus_i2c *i2c,
                           uint8_t dev_addr);
int ctr_drv_ads122c04_reset(struct ctr_drv_ads122c04 *ctx);
int ctr_drv_ads122c04_start_sync(struct ctr_drv_ads122c04 *ctx);
int ctr_drv_ads122c04_powerdown(struct ctr_drv_ads122c04 *ctx);
int ctr_drv_ads122c04_read_reg(struct ctr_drv_ads122c04 *ctx, uint8_t addr, uint8_t *data);
int ctr_drv_ads122c04_write_reg(struct ctr_drv_ads122c04 *ctx, uint8_t addr, uint8_t data);
int ctr_drv_ads122c04_read_data(struct ctr_drv_ads122c04 *ctx, int32_t *data);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRV_ADS122C04_H_ */
