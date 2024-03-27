/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_SUBSYS_CTR_CLOUD_MSG_H_
#define CHESTER_SUBSYS_CTR_CLOUD_MSG_H_

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#include <zcbor_common.h>

/* Zephyr includes */
#include <zephyr/shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UL_CREATE_SESSION  0x00
#define UL_GET_TIMESTAMP   0x01
#define UL_UPLOAD_CONFIG   0x02
#define UL_UPLOAD_DECODER  0x03
#define UL_UPLOAD_ENCODER  0x04
#define UL_UPLOAD_STATS    0x05
#define UL_UPLOAD_DATA     0x06
#define UL_UPLOAD_SHELL    0x07
#define UL_UPLOAD_FIRMWARE 0x08

#define DL_SET_SESSION       0x80
#define DL_SET_TIMESTAMP     0x81
#define DL_DOWNLOAD_CONFIG   0x82
#define DL_DOWNLOAD_DATA     0x86
#define DL_DOWNLOAD_SHELL    0x87
#define DL_DOWNLOAD_FIRMWARE 0x88

// create remote shell  naplanuj shell za 30min

#define DL_REQUEST_REBOOT 0xff

typedef uint8_t ctr_cloud_uuid_t[16];

struct ctr_cloud_msg_dlconfig {
	int lines;
	struct ctr_buf *buf;
	zcbor_state_t zs[3];
};

struct ctr_cloud_msg_dlshell {
	int commands;
	struct ctr_buf *buf;
	zcbor_state_t zs[3 + 2];
	ctr_cloud_uuid_t message_id;
};

struct ctr_cloud_msg_upshell {
	struct ctr_buf *buf;
	zcbor_state_t zs[2];
};

struct ctr_cloud_msg_dlfirmware {
	char target[8];
	char type[10];
	ctr_cloud_uuid_t id;
	uint32_t offset;
	uint32_t length;
	const uint8_t *data;
	uint32_t firmware_size;
};

struct ctr_cloud_upfirmware {
	char target[8];
	char type[10];
	uint8_t *id;
	uint32_t offset;
	uint32_t max_length;
	const char *firmware;
	const char *error;
};

int ctr_cloud_msg_pack_create_session(struct ctr_buf *buf);
int ctr_cloud_msg_unpack_set_session(struct ctr_buf *buf, struct ctr_cloud_session *session);

int ctr_cloud_msg_pack_get_timestamp(struct ctr_buf *buf);
int ctr_cloud_msg_unpack_set_timestamp(struct ctr_buf *buf, int64_t *timestamp);

int ctr_cloud_msg_pack_decoder(struct ctr_buf *buf, uint64_t hash, const uint8_t *decoder_buf,
			       size_t decoder_len);
int ctr_cloud_msg_pack_encoder(struct ctr_buf *buf, uint64_t hash, const uint8_t *encoder_buf,
			       size_t encoder_len);

int ctr_cloud_msg_pack_stats(struct ctr_buf *buf);

int ctr_cloud_msg_pack_config(struct ctr_buf *buf);
int ctr_cloud_msg_unpack_config(struct ctr_buf *buf, struct ctr_cloud_msg_dlconfig *config);

int ctr_cloud_msg_get_hash(struct ctr_buf *buf, uint64_t *hash);

// for working with ctr_cloud_msg_dlconfig
int ctr_cloud_msg_dlconfig_reset(struct ctr_cloud_msg_dlconfig *config);
int ctr_cloud_msg_dlconfig_get_next_line(struct ctr_cloud_msg_dlconfig *config,
					 struct ctr_buf *line);

int ctr_cloud_msg_unpack_dlshell(struct ctr_buf *buf, struct ctr_cloud_msg_dlshell *msg);
int ctr_cloud_msg_dlshell_reset(struct ctr_cloud_msg_dlshell *dlshell);
int ctr_cloud_msg_dlshell_get_next_command(struct ctr_cloud_msg_dlshell *dlshell,
					   struct ctr_buf *command);

int ctr_cloud_msg_pack_upshell_start(struct ctr_cloud_msg_upshell *upshell, struct ctr_buf *buf,
				     const ctr_cloud_uuid_t message_id);
int ctr_cloud_msg_pack_upshell_add_response(struct ctr_cloud_msg_upshell *upshell,
					    const char *command, const int result,
					    const char *output);
int ctr_cloud_msg_pack_upshell_end(struct ctr_cloud_msg_upshell *upshell);

int ctr_cloud_msg_pack_firmware(struct ctr_buf *buf, const struct ctr_cloud_upfirmware *upfirmware);
int ctr_cloud_msg_unpack_dlfirmware(struct ctr_buf *buf,
				    struct ctr_cloud_msg_dlfirmware *dlfirmware);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_CLOUD_MSG_H_ */
