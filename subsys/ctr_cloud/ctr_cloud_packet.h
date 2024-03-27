/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_PACKET_H_
#define CHESTER_SUBSYS_CTR_CLOUD_PACKET_H_

/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTR_CLOUD_PACKET_HASH_SIZE          8
#define CTR_CLOUD_PACKET_SERIAL_NUMBER_SIZE 4
#define CTR_CLOUD_PACKET_HEADER_SIZE        2
#define CTR_CLOUD_PACKET_MIN_SIZE                                                                  \
	(CTR_CLOUD_PACKET_HASH_SIZE + CTR_CLOUD_PACKET_SERIAL_NUMBER_SIZE +                        \
	 CTR_CLOUD_PACKET_HEADER_SIZE)
#define CTR_CLOUD_PACKET_MAX_SIZE 508
#define CTR_CLOUD_DATA_MAX_SIZE   (CTR_CLOUD_PACKET_MAX_SIZE - CTR_CLOUD_PACKET_MIN_SIZE)

#define CTR_CLOUD_PACKET_FLAG_FIRST 0x08
#define CTR_CLOUD_PACKET_FLAG_LAST  0x04
#define CTR_CLOUD_PACKET_FLAG_ACK   0x02
#define CTR_CLOUD_PACKET_FLAG_POLL  0x01

struct ctr_cloud_packet {
	uint32_t serial_number;
	uint16_t sequence;
	uint8_t flags;
	uint8_t *data;
	size_t data_len;
};

int ctr_cloud_packet_pack(struct ctr_cloud_packet *pck, uint8_t claim_token[16],
			  struct ctr_buf *buf);

int ctr_cloud_packet_unpack(struct ctr_cloud_packet *pck, uint8_t claim_token[16],
			    struct ctr_buf *buf);

const char *ctr_cloud_packet_flags_to_str(uint8_t flags);

uint16_t ctr_cloud_packet_sequence_inc(uint16_t sequence);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_PACKET_H_ */
