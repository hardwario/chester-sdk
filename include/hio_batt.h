#ifndef HIO_BATT_H
#define HIO_BATT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hio_batt_result {
	uint16_t voltage_rest_mv;
	uint16_t voltage_load_mv;
	uint16_t current_load_ma;
};

int hio_batt_measure(struct hio_batt_result *result);

#ifdef __cplusplus
}
#endif

#endif
