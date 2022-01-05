#ifndef CHESTER_INCLUDE_CTR_INFO_H_
#define CHESTER_INCLUDE_CTR_INFO_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_info_get_serial_number(uint32_t *sn);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_INFO_H_ */
