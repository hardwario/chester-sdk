#ifndef CHESTER_INCLUDE_DRV_ADS122C04_H_
#define CHESTER_INCLUDE_DRV_ADS122C04_H_

#include <hio_bus_i2c.h>

/* Standard includes */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hio_drv_ads122c04 {
	struct hio_bus_i2c *i2c;
	uint8_t dev_addr;
};

int hio_drv_ads122c04_init(struct hio_drv_ads122c04 *ctx, struct hio_bus_i2c *i2c,
                           uint8_t dev_addr);
int hio_drv_ads122c04_reset(struct hio_drv_ads122c04 *ctx);
int hio_drv_ads122c04_start_sync(struct hio_drv_ads122c04 *ctx);
int hio_drv_ads122c04_powerdown(struct hio_drv_ads122c04 *ctx);
int hio_drv_ads122c04_read_reg(struct hio_drv_ads122c04 *ctx, uint8_t addr, uint8_t *data);
int hio_drv_ads122c04_write_reg(struct hio_drv_ads122c04 *ctx, uint8_t addr, uint8_t data);
int hio_drv_ads122c04_read_data(struct hio_drv_ads122c04 *ctx, int32_t *data);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRV_ADS122C04_H_ */
