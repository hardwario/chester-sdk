#ifndef PACKET_H_
#define PACKET_H_

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void packet_clear(void);
int packet_push(uint8_t *data, size_t len);
void packet_get_pushed_count(size_t *count);
int packet_get_next_size(size_t *len);
int packet_pop(uint8_t *buf, size_t buf_len, size_t *len, int *rssi_dbm);

#ifdef __cplusplus
}
#endif

#endif /* PACKET_H_ */
