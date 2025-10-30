#include "packet.h"
#include "wmbus.h"

/* Zephyr includes */
#include <zephyr/logging/log.h>

/* Standard includes */
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(packet, LOG_LEVEL_DBG);

#define PACKET_INDEX_SIZE 0

/*

WURTH wmbus module adds one byte RSSI information on the end of the packet.
It also increases L (len) field of the WMBUS packet over uart.

With packet_append we append this whole packet including L field increased by one
and with RSSI at the end of the packet.
When retreving packet we remove the RSSI, decrement packet length by one and return RSSI as a
separate function parameter so we can place RSSI as a separate item in CBOR.

*/

#define PAYLOAD_BUF_SIZE (DEVICE_MAX_COUNT * DEVICE_MAX_PAYLOAD)
uint8_t packet_buf[PAYLOAD_BUF_SIZE];
int packet_buf_len = 0;
int packet_count = 0;

int packet_pop_offset = 0;
void packet_clear(void)
{
	packet_buf_len = 0;
	packet_pop_offset = 0;
	packet_count = 0;
}

int packet_push(uint8_t *data, size_t len)
{
	if (len + packet_buf_len > PAYLOAD_BUF_SIZE) {
		return -ENOMEM;
	}

	memcpy(&packet_buf[packet_buf_len], data, len);

	packet_buf_len += len;
	packet_count++;

	LOG_INF("packet %d, buffered %d, buf_size %d", len, packet_buf_len, sizeof(packet_buf));

	return 0;
}

void packet_get_pushed_count(size_t *count)
{
	*count = packet_count;
}

int packet_get_next_size(size_t *len)
{
	if (packet_buf_len > packet_pop_offset) {
		/* Add L field itself, subtract RSSI */
		*len = packet_buf[packet_pop_offset + PACKET_INDEX_SIZE];
		return 0;
	} else {
		*len = 0;
		return -ENOMEM;
	}
}

int packet_pop(uint8_t *buf, size_t buf_len, size_t *len, int *rssi_dbm)
{
	int ret;

	*len = 0;
	*rssi_dbm = 0;

	size_t next_packet_size;
	ret = packet_get_next_size(&next_packet_size);

	if (ret) {
		LOG_ERR("Call `packet_get_next_size` failed: %d", ret);
		return ret;
	}

	if (next_packet_size > buf_len) {
		LOG_ERR("Target buffer too small, skipping packet");
		/* skip also RSSI, increment one */
		packet_pop_offset += next_packet_size + 1;
		return -ENOMEM;
	}

	memcpy(buf, &packet_buf[packet_pop_offset], next_packet_size);
	*rssi_dbm = wmbus_convert_rssi_to_dbm(packet_buf[packet_pop_offset + next_packet_size]);
	*len = next_packet_size;

	/* skip also RSSI, increment one */
	packet_pop_offset += next_packet_size + 1;

	/* Decrement L field because we removed RSSI byte on the packet's end */
	buf[0]--;

	return 0;
}
