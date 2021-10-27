#ifndef CHESTER_INCLUDE_BATT_H_
#define CHESTER_INCLUDE_BATT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ctr_batt_result {
	uint16_t voltage_rest_mv;
	uint16_t voltage_load_mv;
	uint16_t current_load_ma;
};

int ctr_batt_measure(struct ctr_batt_result *result);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_BATT_H_ */
