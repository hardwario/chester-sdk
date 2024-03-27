/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_cloud_msg.h"
#include "ctr_cloud_util.h"

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/sys/byteorder.h>

/* CHESTER includes */
#include <chester/ctr_buf.h>
#include <chester/ctr_info.h>
#include <chester/ctr_lte_v2.h>
#include <chester/drivers/ctr_z.h>
#include <zcbor_common.h>
#include <zcbor_decode.h>
#include <zcbor_encode.h>

#define UL_SESSION_KEY_WATCHDOG_TIMEOUT    0x00
#define UL_SESSION_KEY_SERIAL_NUMBER       0x10
#define UL_SESSION_KEY_VENDOR_NAME         0x01
#define UL_SESSION_KEY_PRODUCT_NAME        0x02
#define UL_SESSION_KEY_HW_VARIANT          0x03
#define UL_SESSION_KEY_HW_REVISION         0x04
#define UL_SESSION_KEY_APP_FW_BUNDLE       0x05
#define UL_SESSION_KEY_APP_FW_NAME         0x06
#define UL_SESSION_KEY_APP_FW_VERSION      0x07
#define UL_SESSION_KEY_BLE_PASSKEY         0x08
#define UL_SESSION_KEY_LTE_IMSI            0x09
#define UL_SESSION_KEY_LTE_IMEI            0x0a
#define UL_SESSION_KEY_LTE_FW_VERSION      0x0b // AT#XVERSION
#define UL_SESSION_KEY_CTR_Z_SERIAL_NUMBER 0x0c
#define UL_SESSION_KEY_CTR_Z_HW_REVISION   0x0d
#define UL_SESSION_KEY_CTR_Z_HW_VARIANT    0x0e
#define UL_SESSION_KEY_CTR_Z_FW_VERSION    0x0f

#define DL_SESSION_KEY_ID           0x00
#define DL_SESSION_KEY_DECODER_HASH 0x01
#define DL_SESSION_KEY_ENCODER_HASH 0x02
#define DL_SESSION_KEY_CONFIG_HASH  0x03
#define DL_SESSION_KEY_TIMESTAMP    0x04
#define DL_SESSION_KEY_DEVICE_ID    0x05
#define DL_SESSION_KEY_DEVICE_NAME  0x06

#define UL_STATS_KEY_UPTIME         0x00
#define UL_STATS_KEY_NETWORK_EEST   0x01
#define UL_STATS_KEY_NETWORK_ECL    0x02
#define UL_STATS_KEY_NETWORK_RSRP   0x03
#define UL_STATS_KEY_NETWORK_RSRQ   0x04
#define UL_STATS_KEY_NETWORK_SNR    0x05
#define UL_STATS_KEY_NETWORK_PLMN   0x06
#define UL_STATS_KEY_NETWORK_CID    0x07
#define UL_STATS_KEY_NETWORK_BAND   0x08
#define UL_STATS_KEY_NETWORK_EARFCN 0x09

#define UL_CONFIG_HEADER_NOCOMPRESSION 0x00

#define DL_SHELL_KEY_COMMANDS   0x00
#define DL_SHELL_KEY_MESSAGE_ID 0x01

#define UL_SHELL_KEY_RESPONSES        0x00
#define UL_SHELL_KEY_MESSAGE_ID       0x01
#define UL_SHELL_RESPONSE_KEY_COMMAND 0x00
#define UL_SHELL_RESPONSE_KEY_RESULT  0x01
#define UL_SHELL_RESPONSE_KEY_OUTPUTS 0x02

#define UL_FIRMWARE_KEY_TARGET     0x00
#define UL_FIRMWARE_KEY_TYPE       0x01
#define UL_FIRMWARE_KEY_ID         0x02
#define UL_FIRMWARE_KEY_OFFSET     0x03
#define UL_FIRMWARE_KEY_MAX_LENGTH 0x04
#define UL_FIRMWARE_KEY_FIRMWARE   0x05
#define UL_FIRMWARE_KEY_ERROR      0x06

#define DL_FIRMWARE_KEY_TARGET        0x00
#define DL_FIRMWARE_KEY_TYPE          0x01
#define DL_FIRMWARE_KEY_ID            0x02
#define DL_FIRMWARE_KEY_OFFSET        0x03
#define DL_FIRMWARE_KEY_LENGTH        0x04
#define DL_FIRMWARE_KEY_DATA          0x05
#define DL_FIRMWARE_KEY_FIRMWARE_SIZE 0x06

#define TSTRCPY(STR, TSTR)                                                                         \
	memcpy(STR, TSTR.value, MIN(TSTR.len, sizeof(STR) - 1));                                   \
	STR[MIN(TSTR.len, sizeof(STR) - 1)] = '\0';

// memcpy(session->device_id, tstr.value,
// 				       MIN(tstr.len, sizeof(session->device_id) - 1));
// 				session->device_id[MIN(tstr.len, sizeof(session->device_id) - 1)] =
// 					'\0';
// 			}

LOG_MODULE_REGISTER(ctr_cloud_msg, CONFIG_CTR_CLOUD_LOG_LEVEL);

int ctr_cloud_msg_pack_create_session(struct ctr_buf *buf)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_CREATE_SESSION);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	uint8_t *p = ctr_buf_get_mem(buf) + 1;

	ZCBOR_STATE_E(zs, 0, p, ctr_buf_get_free(buf) - 1, 1);

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, UL_SESSION_KEY_WATCHDOG_TIMEOUT);
	zcbor_uint32_put(zs, 0); // time in seconds

	uint32_t serial_number = 0;
	ctr_info_get_serial_number_uint32(&serial_number);

	zcbor_uint32_put(zs, UL_SESSION_KEY_SERIAL_NUMBER);
	zcbor_uint32_put(zs, serial_number);

	char *vendor_name;
	ctr_info_get_vendor_name(&vendor_name);

	zcbor_uint32_put(zs, UL_SESSION_KEY_VENDOR_NAME);
	zcbor_tstr_put_term(zs, vendor_name);

	char *product_name;
	ctr_info_get_product_name(&product_name);

	zcbor_uint32_put(zs, UL_SESSION_KEY_PRODUCT_NAME);
	zcbor_tstr_put_term(zs, product_name);

	char *hw_variant;
	ctr_info_get_hw_variant(&hw_variant);

	zcbor_uint32_put(zs, UL_SESSION_KEY_HW_VARIANT);
	zcbor_tstr_put_term(zs, hw_variant);

	char *hw_revision;
	ctr_info_get_hw_revision(&hw_revision);

	zcbor_uint32_put(zs, UL_SESSION_KEY_HW_REVISION);
	zcbor_tstr_put_term(zs, hw_revision);

	char *fw_bundle;
	ctr_info_get_fw_bundle(&fw_bundle);

	zcbor_uint32_put(zs, UL_SESSION_KEY_APP_FW_BUNDLE);
	zcbor_tstr_put_term(zs, fw_bundle);

	char *fw_name;
	ctr_info_get_fw_name(&fw_name);

	zcbor_uint32_put(zs, UL_SESSION_KEY_APP_FW_NAME);
	zcbor_tstr_put_term(zs, fw_name);

	char *fw_version;
	ctr_info_get_fw_version(&fw_version);

	zcbor_uint32_put(zs, UL_SESSION_KEY_APP_FW_VERSION);
	zcbor_tstr_put_term(zs, fw_version);

	char *ble_passkey;
	ctr_info_get_ble_passkey(&ble_passkey);

	zcbor_uint32_put(zs, UL_SESSION_KEY_BLE_PASSKEY);
	zcbor_tstr_put_term(zs, ble_passkey);

	uint64_t imei;
	ctr_lte_v2_get_imei(&imei);

	zcbor_uint32_put(zs, UL_SESSION_KEY_LTE_IMEI);
	zcbor_uint64_put(zs, imei);

	uint64_t imsi;
	ctr_lte_v2_get_imsi(&imsi);

	zcbor_uint32_put(zs, UL_SESSION_KEY_LTE_IMSI);
	zcbor_uint64_put(zs, imsi);

	char *modem_fw_version;
	ctr_lte_v2_get_modem_fw_version(&modem_fw_version);

	zcbor_uint32_put(zs, UL_SESSION_KEY_LTE_FW_VERSION);
	zcbor_tstr_put_term(zs, modem_fw_version);

#if defined(CONFIG_SHIELD_CTR_Z)

	uint32_t ctr_z_serial_number = 0;
	uint16_t ctr_z_hw_revision = 0;
	uint32_t ctr_z_hw_variant = 0;
	uint32_t ctr_z_fw_version = 0;

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_WRN("ctr_z not ready");
	} else {
		ret = ctr_z_get_serial_number(dev, &ctr_z_serial_number);
		if (ret) {
			LOG_WRN("Call `ctr_z_get_serial_number` failed: %d", ret);
		}

		ret = ctr_z_get_hw_revision(dev, &ctr_z_hw_revision);
		if (ret) {
			LOG_WRN("Call `ctr_z_get_hw_revision` failed: %d", ret);
		}
		ret = ctr_z_get_hw_variant(dev, &ctr_z_hw_variant);
		if (ret) {
			LOG_WRN("Call `ctr_z_get_hw_variant` failed: %d", ret);
		}
		ret = ctr_z_get_fw_version(dev, &ctr_z_fw_version);
		if (ret) {
			LOG_ERR("Call `ctr_z_get_fw_version` failed: %d", ret);
			return ret;
		}
	}

	char tmp[64] = {0};

	zcbor_uint32_put(zs, UL_SESSION_KEY_CTR_Z_SERIAL_NUMBER);
	snprintf(tmp, sizeof(tmp), "%d", ctr_z_serial_number);
	zcbor_tstr_put_term(zs, tmp);

	zcbor_uint32_put(zs, UL_SESSION_KEY_CTR_Z_HW_REVISION);
	snprintf(tmp, sizeof(tmp), "R%u.%u", (ctr_z_hw_revision >> 8) & 0xff,
		 ctr_z_hw_revision & 0xff);
	zcbor_tstr_put_term(zs, tmp);

	zcbor_uint32_put(zs, UL_SESSION_KEY_CTR_Z_HW_VARIANT);
	snprintf(tmp, sizeof(tmp), "0x%04x", ctr_z_hw_variant);
	zcbor_tstr_put_term(zs, tmp);

	zcbor_uint32_put(zs, UL_SESSION_KEY_CTR_Z_FW_VERSION);
	snprintf(tmp, sizeof(tmp), "v%u.%u.%u", (ctr_z_fw_version >> 24) & 0xff,
		 (ctr_z_fw_version >> 16) & 0xff, (ctr_z_fw_version >> 8) & 0xff);
	zcbor_tstr_put_term(zs, tmp);

#endif /* defined(CONFIG_SHIELD_CTR_Z) */

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	ctr_buf_seek(buf, 1 + (zs->payload - p));

	return 0;
}

int ctr_cloud_msg_unpack_set_session(struct ctr_buf *buf, struct ctr_cloud_session *session)
{
	if (session == NULL) {
		LOG_ERR("Invalid session pointer");
		return -EINVAL;
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	if (p[0] != DL_SET_SESSION) {
		LOG_ERR("Invalid message type: %d", p[0]);
		return -EPROTO;
	}

	memset(session, 0, sizeof(struct ctr_cloud_session));

	ZCBOR_STATE_D(zs, 1, p + 1, ctr_buf_get_used(buf) - 1, 1);

	if (!zcbor_map_start_decode(zs)) {
		return -EBADMSG;
	}

	uint32_t key;
	bool ok;
	struct zcbor_string tstr;

	while (1) {
		if (!zcbor_uint32_decode(zs, &key)) {
			break;
		}

		switch (key) {
		case DL_SESSION_KEY_ID:
			ok = zcbor_uint32_decode(zs, &session->id);
			break;
		case DL_SESSION_KEY_DECODER_HASH:
			ok = zcbor_uint64_decode(zs, &session->decoder_hash);
			break;
		case DL_SESSION_KEY_ENCODER_HASH:
			ok = zcbor_uint64_decode(zs, &session->encoder_hash);
			break;
		case DL_SESSION_KEY_CONFIG_HASH:
			ok = zcbor_uint64_decode(zs, &session->config_hash);
			break;
		case DL_SESSION_KEY_TIMESTAMP:
			ok = zcbor_int64_decode(zs, &session->timestamp);
			break;
		case DL_SESSION_KEY_DEVICE_ID:
			ok = zcbor_tstr_decode(zs, &tstr);
			if (ok) {
				TSTRCPY(session->device_id, tstr);
			}
			break;
		case DL_SESSION_KEY_DEVICE_NAME:
			ok = zcbor_tstr_decode(zs, &tstr);
			if (ok) {
				TSTRCPY(session->device_name, tstr);
			}
			break;
		}
		if (!ok) {
			return -EBADMSG;
		}
	}

	if (!zcbor_map_end_decode(zs)) {
		return -EBADMSG;
	}

	return 0;
}

int ctr_cloud_msg_pack_get_timestamp(struct ctr_buf *buf)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_GET_TIMESTAMP);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}
	return 0;
}

int ctr_cloud_msg_unpack_set_timestamp(struct ctr_buf *buf, int64_t *timestamp)
{
	if (timestamp == NULL) {
		LOG_ERR("Invalid timestamp pointer");
		return -EINVAL;
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	if (p[0] != DL_SET_TIMESTAMP) {
		LOG_ERR("Invalid message type: %d", p[0]);
		return -EPROTO;
	}

	if (ctr_buf_get_used(buf) != 9) {
		LOG_ERR("Unexpected downlink size: %u byte(s)", ctr_buf_get_used(buf));
		return -EINVAL;
	}

	*timestamp = sys_get_be64(&p[1]); // ms

	return 0;
}

int ctr_cloud_msg_pack_decoder(struct ctr_buf *buf, uint64_t hash, const uint8_t *decoder_buf,
			       size_t decoder_len)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_DECODER);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u64_be(buf, hash);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u64_be` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_mem(buf, decoder_buf, decoder_len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}
	return 0;
}

int ctr_cloud_msg_pack_encoder(struct ctr_buf *buf, uint64_t hash, const uint8_t *encoder_buf,
			       size_t encoder_len)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_ENCODER);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u64_be(buf, hash);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u64_be` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_mem(buf, encoder_buf, encoder_len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}
	return 0;
}

int ctr_cloud_msg_pack_stats(struct ctr_buf *buf)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_STATS);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	uint8_t *p = ctr_buf_get_mem(buf) + 1;

	ZCBOR_STATE_E(zs, 0, p, ctr_buf_get_free(buf) - 1, 1);

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, UL_STATS_KEY_UPTIME);
	zcbor_uint64_put(zs, k_uptime_get() / 1000);

	struct ctr_lte_v2_conn_param param;
	ret = ctr_lte_v2_get_conn_param(&param);
	if (ret) {
		LOG_ERR("Call `ctr_lte_v2_get_conn_param` failed: %d", ret);
		return ret;
	}

	if (param.valid) { // posilat kazdou n tou zpravu, nebo po uplinuti casu
		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_EEST);
		zcbor_int32_put(zs, param.eest);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_ECL);
		zcbor_int32_put(zs, param.ecl);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_RSRP);
		zcbor_int32_put(zs, param.rsrp);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_RSRQ);
		zcbor_int32_put(zs, param.rsrq);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_SNR);
		zcbor_int32_put(zs, param.snr);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_PLMN);
		zcbor_int32_put(zs, param.plmn);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_CID);
		zcbor_int32_put(zs, param.cid);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_BAND);
		zcbor_int32_put(zs, param.band);

		zcbor_uint32_put(zs, UL_STATS_KEY_NETWORK_EARFCN);
		zcbor_int32_put(zs, param.earfcn);
	}

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	ctr_buf_seek(buf, 1 + (zs->payload - p));

	return 0;
}

static int pack_shell_output_as_list(zcbor_state_t *zs, const char *output, size_t size)
{
	struct zcbor_string tstr;
	uint8_t *outp = (uint8_t *)output;
	const uint8_t *end = outp + size;

	if (size < 2) {
		LOG_ERR("Invalid shell output size: %u", size);
		return -EINVAL;
	}

	if (outp[0] != '\r' && outp[1] != '\n') {
		LOG_ERR("Invalid shell output format");
		return -EINVAL;
	}

	if (!zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		return -EMSGSIZE;
	}

	tstr.value = outp;
	do {
		if (outp[0] == '\r' && outp[1] == '\n') {
			tstr.len = outp - tstr.value;

			if (tstr.len > 0) {
				if (!zcbor_tstr_encode(zs, &tstr)) {
					LOG_ERR("Call `zcbor_bstr_encode` failed");
					return -EMSGSIZE;
				}
			}

			outp += 2;
			tstr.value = outp;
		}

	} while (++outp < end);

	if (!zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		return -EMSGSIZE;
	}

	return 0;
}

int ctr_cloud_msg_pack_config(struct ctr_buf *buf)
{
	int ret;

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_CONFIG);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	const struct shell *sh = shell_backend_dummy_get_ptr();

	shell_backend_dummy_clear_output(sh);

	ret = shell_execute_cmd(sh, "config show");

	if (ret) {
		LOG_ERR("Call `shell_execute_cmd` failed: %d", ret);
		return ret;
	}

	size_t size;

	const uint8_t *outp = shell_backend_dummy_get_output(sh, &size);
	if (!outp) {
		LOG_ERR("Failed to get shell output");
		return -ENOMEM;
	}

	uint8_t hash[8];
	ctr_cloud_calculate_hash(hash, outp, size);

	ret = ctr_buf_append_mem(buf, hash, sizeof(hash));
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u8(buf, UL_CONFIG_HEADER_NOCOMPRESSION);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	uint8_t *p = ctr_buf_get_mem(buf) + 1 + 8 + 1; // header + hash + compression

	ZCBOR_STATE_E(zs, 0, p, ctr_buf_get_free(buf) - 1, 1);

	ret = pack_shell_output_as_list(zs, outp, size);
	if (ret) {
		LOG_ERR("Call `pack_shell_output_as_list` failed: %d", ret);
		return ret;
	}
	// zcbor_list_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	// struct zcbor_string tstr;
	// const uint8_t *end = outp + size;

	// tstr.value = outp;
	// do {
	// 	if (outp[0] == '\r' && outp[1] == '\n') {
	// 		tstr.len = outp - tstr.value;

	// 		if (tstr.len > 0) {
	// 			ret = zcbor_tstr_encode(zs, &tstr);
	// 			if (!ret) {
	// 				LOG_ERR("Call `zcbor_bstr_encode` failed: %d", ret);
	// 				return ret;
	// 			}
	// 		}

	// 		outp += 2;
	// 		tstr.value = outp;
	// 	}

	// } while (++outp < end);

	// zcbor_list_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	ctr_buf_seek(buf, 1 + 8 + 1 + (zs->payload - p));

	return 0;
}

int ctr_cloud_msg_unpack_config(struct ctr_buf *buf, struct ctr_cloud_msg_dlconfig *config)
{
	if (config == NULL) {
		LOG_ERR("Invalid config pointer");
		return -EINVAL;
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	if (p[0] != DL_DOWNLOAD_CONFIG) {
		LOG_ERR("Invalid message type: %d", p[0]);
		return -EPROTO;
	}

	if (ctr_buf_get_used(buf) < (4)) { // type(1) + header(1) + min cbor(2)
		LOG_ERR("To small size: %u byte(s)", ctr_buf_get_used(buf));
		return -EINVAL;
	}

	uint8_t header = p[1];

	if (header != 0x00) {
		LOG_ERR("Invalid header: 0x%02x", header);
		return -EINVAL;
	}

	memset(config, 0, sizeof(struct ctr_cloud_msg_dlconfig));

	config->buf = buf;

	zcbor_state_t *zs = &config->zs[0];

	zcbor_new_decode_state(zs, sizeof(config->zs) / sizeof(zcbor_state_t), p + 2,
			       ctr_buf_get_used(buf) - 2, 1);

	if (!zcbor_list_start_decode(zs)) {
		return -EBADMSG;
	}

	struct zcbor_string tstr;

	while (1) {
		if (!zcbor_tstr_decode(zs, &tstr)) {
			break;
		}

		if (tstr.len >= CONFIG_SHELL_CMD_BUFF_SIZE) {
			LOG_ERR("To big line: %u byte(s)", tstr.len);
			return -ENOMEM;
		}

		if (tstr.len > 0) {
			config->lines++;
		}
	}

	if (!zcbor_list_end_decode(zs)) {
		return -EBADMSG;
	}

	// re init decoder state, prepare for reading lines
	int ret = ctr_cloud_msg_dlconfig_reset(config);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_dlconfig_reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_msg_get_hash(struct ctr_buf *buf, uint64_t *hash)
{
	if (hash == NULL) {
		LOG_ERR("Invalid hash pointer");
		return -EINVAL;
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	if (ctr_buf_get_used(buf) < 9) {
		LOG_ERR("Unexpected size: %u byte(s)", ctr_buf_get_used(buf));
		return -EINVAL;
	}

	*hash = sys_get_be64(&p[1]);

	return 0;
}

int ctr_cloud_msg_dlconfig_reset(struct ctr_cloud_msg_dlconfig *config)
{
	uint8_t *p = ctr_buf_get_mem(config->buf);

	zcbor_new_decode_state(config->zs, sizeof(config->zs) / sizeof(zcbor_state_t), p + 2,
			       ctr_buf_get_used(config->buf) - 2, 1);

	if (!zcbor_list_start_decode(config->zs)) {
		LOG_ERR("Call `zcbor_list_start_decode` failed");
		return -EBADMSG;
	}

	return 0;
}

int ctr_cloud_msg_dlconfig_get_next_line(struct ctr_cloud_msg_dlconfig *config,
					 struct ctr_buf *line)
{
	struct zcbor_string tstr;

	if (!zcbor_tstr_decode(config->zs, &tstr)) {
		return -ENODATA;
	}

	if (tstr.len > ctr_buf_get_free(line) - 1) {
		LOG_ERR("To big line: %u byte(s)", tstr.len);
		return -ENOMEM;
	}

	char *pline = ctr_buf_get_mem(line) + ctr_buf_get_used(line);

	memcpy(pline, tstr.value, tstr.len);

	int ret = ctr_buf_seek(line, ctr_buf_get_used(line) + tstr.len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_seek` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u8(line, '\0');
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_msg_unpack_dlshell(struct ctr_buf *buf, struct ctr_cloud_msg_dlshell *dlshell)
{
	if (dlshell == NULL) {
		LOG_ERR("Invalid dlshell pointer");
		return -EINVAL;
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	if (p[0] != DL_DOWNLOAD_SHELL) {
		LOG_ERR("Invalid message type: %d", p[0]);
		return -EPROTO;
	}

	memset(dlshell, 0, sizeof(struct ctr_cloud_msg_dlshell));

	dlshell->buf = buf;

	zcbor_new_decode_state(dlshell->zs, sizeof(dlshell->zs) / sizeof(zcbor_state_t), p + 1,
			       ctr_buf_get_used(dlshell->buf) - 1, 1);

	if (!zcbor_map_start_decode(dlshell->zs)) {
		LOG_ERR("Call `zcbor_map_start_decode` failed");
		return -EBADMSG;
	}

	uint32_t key;
	bool ok;
	struct zcbor_string tstr;

	while (1) {
		if (!zcbor_uint32_decode(dlshell->zs, &key)) {
			break; // no more keys
		}

		switch (key) {
		case DL_SHELL_KEY_COMMANDS:
			if (!zcbor_list_start_decode(dlshell->zs)) {
				LOG_ERR("Call `zcbor_array_start_decode` failed");
				return -EBADMSG;
			}
			while (1) {
				if (!zcbor_tstr_decode(dlshell->zs, &tstr)) {
					break;
				}

				if (tstr.len >= CONFIG_SHELL_CMD_BUFF_SIZE) {
					LOG_ERR("To big line: %u byte(s)", tstr.len);
					return -ENOMEM;
				}

				dlshell->commands++;
			}
			if (!zcbor_list_end_decode(dlshell->zs)) {
				LOG_ERR("Call `zcbor_array_end_decode` failed");
				return -EBADMSG;
			}
			break;
		case DL_SHELL_KEY_MESSAGE_ID:
			ok = zcbor_bstr_decode(dlshell->zs, &tstr);
			if (ok) {
				if (tstr.len != sizeof(dlshell->message_id)) {
					LOG_ERR("Invalid message id size: %u byte(s)", tstr.len);
					return -EINVAL;
				}
				memcpy(dlshell->message_id, tstr.value,
				       sizeof(dlshell->message_id));
			}
			break;
		}
		if (!ok) {
			LOG_ERR("Value decode failed for key: %u", key);
			return -EBADMSG;
		}
	}

	if (!zcbor_map_end_decode(dlshell->zs)) {
		LOG_ERR("Call `zcbor_map_end_decode` failed");
		return -EBADMSG;
	}

	int ret = ctr_cloud_msg_dlshell_reset(dlshell);
	if (ret) {
		LOG_ERR("Call `ctr_cloud_msg_dlshell_reset` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_msg_dlshell_reset(struct ctr_cloud_msg_dlshell *dlshell)
{
	uint8_t *p = ctr_buf_get_mem(dlshell->buf);

	zcbor_new_decode_state(dlshell->zs, sizeof(dlshell->zs) / sizeof(zcbor_state_t), p + 1,
			       ctr_buf_get_used(dlshell->buf) - 1, 1);

	if (!zcbor_map_start_decode(dlshell->zs)) {
		LOG_ERR("Call `zcbor_map_start_decode` failed");
		return -EBADMSG;
	}

	uint32_t key;

	while (1) {
		if (!zcbor_uint32_decode(dlshell->zs, &key)) {
			return -EBADMSG;
		}
		if (key == DL_SHELL_KEY_COMMANDS) {
			if (!zcbor_list_start_decode(dlshell->zs)) {
				LOG_ERR("Call `zcbor_array_start_decode` failed");
				return -EBADMSG;
			}
			return 0;
		}
	}

	return -EBADMSG;
}

int ctr_cloud_msg_dlshell_get_next_command(struct ctr_cloud_msg_dlshell *dlshell,
					   struct ctr_buf *command)
{
	struct zcbor_string tstr;

	if (!zcbor_tstr_decode(dlshell->zs, &tstr)) {
		return -ENODATA;
	}

	if (tstr.len > ctr_buf_get_free(command)) {
		LOG_ERR("To big line: %u byte(s)", tstr.len);
		return -ENOMEM;
	}

	int ret = ctr_buf_append_mem(command, tstr.value, tstr.len);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_mem` failed: %d", ret);
		return ret;
	}

	ret = ctr_buf_append_u8(command, '\0'); // add null terminate
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	return 0;
}

int ctr_cloud_msg_pack_upshell_start(struct ctr_cloud_msg_upshell *upshell, struct ctr_buf *buf,
				     const ctr_cloud_uuid_t message_id)
{
	int ret;

	if (upshell == NULL) {
		LOG_ERR("Invalid upshell pointer");
		return -EINVAL;
	}

	if (buf == NULL) {
		LOG_ERR("Invalid buf pointer");
		return -EINVAL;
	}

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_SHELL);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	memset(upshell, 0, sizeof(struct ctr_cloud_msg_upshell));

	upshell->buf = buf;

	uint8_t *p = ctr_buf_get_mem(buf) + 1;

	zcbor_new_state(upshell->zs, 0, p, ctr_buf_get_free(buf) - 1, 1);

	if (!zcbor_map_start_encode(upshell->zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		LOG_ERR("Call `zcbor_map_start_encode` failed");
		return -EBADMSG;
	}

	if (message_id) {
		if (!zcbor_uint32_put(upshell->zs, UL_SHELL_KEY_MESSAGE_ID)) {
			LOG_ERR("Call `zcbor_uint32_put` failed");
			return -EBADMSG;
		}
		if (!zcbor_bstr_encode_ptr(upshell->zs, message_id, sizeof(ctr_cloud_uuid_t))) {
			LOG_ERR("Call `zcbor_bstr_encode_ptr` failed");
			return -EBADMSG;
		}
	}

	if (!zcbor_uint32_put(upshell->zs, UL_SHELL_KEY_RESPONSES)) {
		LOG_ERR("Call `zcbor_uint32_put` failed");
		return -EBADMSG;
	}

	if (!zcbor_list_start_encode(upshell->zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		LOG_ERR("Call `zcbor_list_start_encode` failed");
		return -EBADMSG;
	}

	return 0;
}
int ctr_cloud_msg_pack_upshell_add_response(struct ctr_cloud_msg_upshell *upshell,
					    const char *command, const int result,
					    const char *output)
{
	if (upshell == NULL) {
		LOG_ERR("Invalid upshell pointer");
		return -EINVAL;
	}

	if (command == NULL) {
		LOG_ERR("Invalid cmd pointer");
		return -EINVAL;
	}

	if (!zcbor_map_start_encode(upshell->zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		LOG_ERR("Call `zcbor_map_start_encode` failed");
		return -EBADMSG;
	}

	if (!zcbor_uint32_put(upshell->zs, UL_SHELL_RESPONSE_KEY_COMMAND)) {
		LOG_ERR("Call `zcbor_uint32_put` failed");
		return -EBADMSG;
	}

	if (!zcbor_tstr_put_term(upshell->zs, command)) {
		LOG_ERR("Call `zcbor_tstr_put_term` failed");
		return -EBADMSG;
	}

	if (result) {
		if (!zcbor_uint32_put(upshell->zs, UL_SHELL_RESPONSE_KEY_RESULT)) {
			LOG_ERR("Call `zcbor_uint32_put` failed");
			return -EBADMSG;
		}

		if (!zcbor_int32_put(upshell->zs, result)) {
			LOG_ERR("Call `zcbor_int32_put` failed");
			return -EBADMSG;
		}
	}

	if (output) {
		if (!zcbor_uint32_put(upshell->zs, UL_SHELL_RESPONSE_KEY_OUTPUTS)) {
			LOG_ERR("Call `zcbor_uint32_put` failed");
			return -EBADMSG;
		}
		int ret = pack_shell_output_as_list(upshell->zs, output, strlen(output));
		if (ret) {
			LOG_ERR("Call `pack_shell_output_as_list` failed: %d", ret);
			return ret;
		}
	}

	if (!zcbor_map_end_encode(upshell->zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		LOG_ERR("Call `zcbor_map_end_encode` failed");
		return -EBADMSG;
	}

	return 0;
}

int ctr_cloud_msg_pack_upshell_end(struct ctr_cloud_msg_upshell *upshell)
{
	if (upshell == NULL) {
		LOG_ERR("Invalid upshell pointer");
		return -EINVAL;
	}

	if (!zcbor_list_end_encode(upshell->zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		LOG_ERR("Call `zcbor_list_end_encode` failed");
		return -EBADMSG;
	}

	if (!zcbor_map_end_encode(upshell->zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH)) {
		LOG_ERR("Call `zcbor_map_end_encode` failed");
		return -EBADMSG;
	}

	uint8_t *p = ctr_buf_get_mem(upshell->buf) + 1;

	ctr_buf_seek(upshell->buf, 1 + (upshell->zs->payload - p));

	return 0;
}

int ctr_cloud_msg_pack_firmware(struct ctr_buf *buf, const struct ctr_cloud_upfirmware *upfirmware)
{
	int ret;

	if (strncmp(upfirmware->target, "app", 3) != 0) {
		LOG_ERR("Invalid target: %s", upfirmware->target);
		return -EINVAL;
	}

	if (strncmp(upfirmware->type, "download", 5) == 0) {
		if (upfirmware->firmware == NULL) {
			LOG_ERR("Invalid firmware pointer");
			return -EINVAL;
		}
		if (strlen(upfirmware->firmware) == 0) {
			LOG_ERR("Invalid firmware length is zero");
			return -EINVAL;
		}
		if (upfirmware->max_length == 0) {
			LOG_ERR("Invalid max_length is zero");
			return -EINVAL;
		}
	}

	if (strncmp(upfirmware->type, "next", 4) == 0) {
		if (upfirmware->max_length == 0) {
			LOG_ERR("Invalid max_length is zero");
			return -EINVAL;
		}
	}

	ret = ctr_buf_append_u8(buf, UL_UPLOAD_FIRMWARE);
	if (ret) {
		LOG_ERR("Call `ctr_buf_append_u8` failed: %d", ret);
		return ret;
	}

	uint8_t *p = ctr_buf_get_mem(buf) + 1;

	ZCBOR_STATE_E(zs, 0, p, ctr_buf_get_free(buf) - 1, 1);

	zcbor_map_start_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	zcbor_uint32_put(zs, UL_FIRMWARE_KEY_TARGET);
	zcbor_tstr_put_term(zs, upfirmware->target);

	zcbor_uint32_put(zs, UL_FIRMWARE_KEY_TYPE);
	zcbor_tstr_put_term(zs, upfirmware->type);

	if (upfirmware->id != NULL) {
		zcbor_uint32_put(zs, UL_FIRMWARE_KEY_ID);
		zcbor_bstr_encode_ptr(zs, upfirmware->id, sizeof(ctr_cloud_uuid_t));
	}

	if (upfirmware->offset) {
		zcbor_uint32_put(zs, UL_FIRMWARE_KEY_OFFSET);
		zcbor_uint32_put(zs, upfirmware->offset);
	}

	if (upfirmware->max_length) {
		zcbor_uint32_put(zs, UL_FIRMWARE_KEY_MAX_LENGTH);
		zcbor_uint32_put(zs, upfirmware->max_length);
	}

	if (upfirmware->firmware != NULL) {
		zcbor_uint32_put(zs, UL_FIRMWARE_KEY_FIRMWARE);
		zcbor_tstr_put_term(zs, upfirmware->firmware);
	}

	if (upfirmware->error != NULL) {
		zcbor_uint32_put(zs, UL_FIRMWARE_KEY_ERROR);
		zcbor_tstr_put_term(zs, upfirmware->error);
	}

	zcbor_map_end_encode(zs, ZCBOR_VALUE_IS_INDEFINITE_LENGTH);

	ctr_buf_seek(buf, 1 + (zs->payload - p));

	return 0;
}

int ctr_cloud_msg_unpack_dlfirmware(struct ctr_buf *buf,
				    struct ctr_cloud_msg_dlfirmware *dlfirmware)
{
	if (dlfirmware == NULL) {
		LOG_ERR("Invalid dlfirmware pointer");
		return -EINVAL;
	}

	uint8_t *p = ctr_buf_get_mem(buf);

	if (p[0] != DL_DOWNLOAD_FIRMWARE) {
		LOG_ERR("Invalid message type: %d", p[0]);
		return -EPROTO;
	}

	memset(dlfirmware, 0, sizeof(struct ctr_cloud_msg_dlfirmware));

	ZCBOR_STATE_D(zs, 1, p + 1, ctr_buf_get_used(buf) - 1, 1);

	if (!zcbor_map_start_decode(zs)) {
		return -EBADMSG;
	}

	uint32_t key;
	bool ok;
	struct zcbor_string tstr;
	size_t data_len = 0;

	while (1) {
		if (!zcbor_uint32_decode(zs, &key)) {
			break;
		}

		switch (key) {
		case DL_FIRMWARE_KEY_TARGET:
			ok = zcbor_tstr_decode(zs, &tstr);
			if (ok) {
				TSTRCPY(dlfirmware->target, tstr);
			}
			break;
		case DL_FIRMWARE_KEY_TYPE:
			ok = zcbor_tstr_decode(zs, &tstr);
			if (ok) {
				TSTRCPY(dlfirmware->type, tstr);
			}
			break;
		case DL_FIRMWARE_KEY_ID:
			ok = zcbor_bstr_decode(zs, &tstr);
			if (ok) {
				if (tstr.len != sizeof(dlfirmware->id)) {
					LOG_ERR("Invalid firmware id size: %u byte(s)", tstr.len);
					return -EINVAL;
				}
				memcpy(dlfirmware->id, tstr.value, sizeof(dlfirmware->id));
			}
			break;
		case DL_FIRMWARE_KEY_OFFSET:
			ok = zcbor_uint32_decode(zs, &dlfirmware->offset);
			break;
		case DL_FIRMWARE_KEY_LENGTH:
			ok = zcbor_uint32_decode(zs, &dlfirmware->length);
			break;
		case DL_FIRMWARE_KEY_DATA:
			ok = zcbor_bstr_decode(zs, &tstr);
			if (ok) {
				dlfirmware->data = tstr.value;
				data_len = tstr.len;
			}
			break;
		case DL_FIRMWARE_KEY_FIRMWARE_SIZE:
			ok = zcbor_uint32_decode(zs, &dlfirmware->firmware_size);
			break;
		}
		if (!ok) {
			LOG_ERR("Value decode failed for key: %u", key);
			return -EBADMSG;
		}
	}

	if (!zcbor_map_end_decode(zs)) {
		return -EBADMSG;
	}

	if (data_len != dlfirmware->length) {
		LOG_ERR("Invalid data length: %u != %u", data_len, dlfirmware->length);
		return -EINVAL;
	}

	return 0;
}
