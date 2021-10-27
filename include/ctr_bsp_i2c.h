#ifndef CHESTER_INCLUDE_BSP_I2C_H_
#define CHESTER_INCLUDE_BSP_I2C_H_

#include <ctr_bus_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

const struct hio_bus_i2c_driver *hio_bsp_i2c_get_driver(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_BSP_I2C_H_ */
