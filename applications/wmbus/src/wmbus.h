#ifndef WMBUS_H_
#define WMBUS_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>
#include <zephyr/sys/ring_buffer.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_MAX_COUNT   256
#define DEVICE_MAX_PAYLOAD 180

typedef struct wmbus_packet_meta {
	uint32_t address;
	int rssi_dbm;
	uint16_t manufacturer_id;
	uint32_t address_bcd;
	uint8_t device_type;
	uint8_t device_version;
} wmbus_packet_meta;

size_t wmbus_rx_byte(uint8_t b);
uint8_t *wmbus_get_buffer(void);

bool wmbus_set_and_check_address_flag(uint32_t address);
bool wmbus_check_all_received_flags(void);
void wmbus_clear_address_flags(void);
void wmbus_get_config_device_count(size_t *count);

uint32_t wmbus_convert_bcd_to_uint32(uint32_t address);
int wmbus_convert_rssi_to_dbm(uint8_t rssi_raw);

void wmbus_parse_packet(uint8_t *packet, size_t len, wmbus_packet_meta *meta);

int wmbus_init(void);

int wmbus_enable(void);
int wmbus_disable(void);

int wmbus_configure(const struct shell *shell, size_t argc, char **argv);

int wmbus_antenna_set(int ant);

#ifdef __cplusplus
}
#endif

#endif /* WMBUS_H_ */
