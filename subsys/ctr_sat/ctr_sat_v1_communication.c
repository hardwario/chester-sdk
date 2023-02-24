/* CHESTER includes */
#include "astronode_s.h"
#include "ctr_sat_v1_internal.h"

#include <chester/ctr_sat.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(ctr_sat_v1_communication, CONFIG_CTR_SAT_LOG_LEVEL);

static int ctr_sat_check_message_crc(uint8_t response_id, const uint8_t *parameters,
				     size_t parameters_len, const uint8_t *rx_buf,
				     size_t rx_buf_len)
{
	int ret;
	uint16_t actual_crc = crc16_itu_t(0xFFFF, &response_id, sizeof(response_id));
	actual_crc = crc16_itu_t(actual_crc, parameters, parameters_len);

	char crc_bytes[2];

	ret = hex2bin(rx_buf + rx_buf_len - 5, 4, crc_bytes, sizeof(crc_bytes));
	if (ret != sizeof(crc_bytes)) {
		/* possible ret == 0 when hex2bin fail */
		LOG_ERR("Parse error: Mallformed CRC byte value");
		return -E_INVCRC;
	}

	uint16_t message_crc = (((uint16_t)crc_bytes[1]) << 8) | (uint16_t)crc_bytes[0];

	if (message_crc != actual_crc) {
		LOG_ERR("CRC mismatch detected when checking CRC of response message. Expected "
			"CRC: 0x%04x. Actual CRC: 0x%04x",
			message_crc, actual_crc);
		return -E_INVCRC;
	}

	return 0;
}

static int ctr_sat_parse_message_container(uint8_t *response_id, const uint8_t *rx_buf,
					   size_t rx_buf_len)
{
	int ret;

	/* 8 == 1 (start byte) + 2 (code) + 4 (crc) + 1 (stop byte) */
	if (rx_buf_len < 8) {
		LOG_ERR("Parse error: Message too short");
		return -EINVAL;
	}

	if (rx_buf[0] != ASTRONODE_S_START_BYTE) {
		LOG_ERR("Parse error: Unexpected start of message");
		return -EINVAL;
	}

	if (rx_buf[rx_buf_len - 1] != ASTRONODE_S_END_BYTE) {
		LOG_ERR("Parse error: Missing end of message");
		return -EINVAL;
	}

	ret = hex2bin((const char *)(rx_buf + 1), 2, (char *)response_id, sizeof(*response_id));
	if (ret != sizeof(*response_id)) {
		/* possible ret == 0 when hex2bin fail */
		LOG_ERR("Parse error: Mallformed reply ID byte received. "
			"Error %d",
			ret);
		return -EIO;
	}

	return 0;
}

static int ctr_sat_parse_response_data(uint8_t *response_data, size_t response_data_size_max,
				       size_t *response_data_size_parsed, const uint8_t *rx_buf,
				       size_t rx_buf_len, int flags)
{
	int ret;

	size_t to_parse;

	if (FIELD_GET(FLAG_ALLOW_UNEXPECTED_LENGTH, flags)) {
		if ((rx_buf_len % 2) == 1) {
			LOG_ERR("Received asnwer with invalid odd length. Received: %d",
				rx_buf_len);
			return -EINVAL;
		} else if (rx_buf_len < 8) {
			LOG_ERR("Received asnwer is too short. Received: %d", rx_buf_len);
			return -EINVAL;
		}

		/* 8 == 1 (start byte) + 2 (opcode) + 4 (crc) + 1 (stop byte) */
		to_parse = (rx_buf_len - 8) / 2;
	} else {
		/* 8 == 1 (start byte) + 2 (opcode) + 4 (crc) + 1 (stop byte) */
		if (8 + response_data_size_max * 2 != rx_buf_len) {

			LOG_ERR("Received asnwer with invalid length. Expected: %d, "
				"Received: %d",
				8 + response_data_size_max * 2, rx_buf_len);
			return -EINVAL;
		}

		to_parse = response_data_size_max;
	}

	ret = hex2bin(rx_buf + 3, MIN(rx_buf_len - 3, to_parse * 2), response_data, to_parse);
	if (ret != to_parse) {
		LOG_ERR("Call `hex2bin` failed: %d", ret);
		return -EIO; /* ret == 0 when hex2bin fail */
	}

	*response_data_size_parsed = to_parse;

	return 0;
}

static int ctr_sat_parse_error_answer(const uint8_t *rx_buf, size_t rx_buf_len, int flags)
{
	int ret;

	uint8_t error_answer[ASTRONODE_S_ERR_ANSWER_SIZE];
	size_t error_answer_size = sizeof(error_answer);
	size_t parsed_len;
	ret = ctr_sat_parse_response_data(error_answer, error_answer_size, &parsed_len, rx_buf,
					  rx_buf_len, 0);
	if (ret) {
		LOG_ERR("Call `ctr_sat_parse_response_data` failed: %d", ret);
		return ret;
	}

	ret = ctr_sat_check_message_crc(ASTRONODE_S_ANSWER_ERROR, error_answer,
					sizeof(error_answer), rx_buf, rx_buf_len);
	if (ret) {
		LOG_ERR("Call `ctr_sat_check_message_crc` failed: %d", ret);
		return ret;
	}

	uint16_t error_code = sys_get_le16(error_answer);

	if (FIELD_GET(FLAG_NO_WARN_ASTRONODE_ERROR, flags) == 0) {
		LOG_ERR("Astronode command execution failed with error code 0x%04x", error_code);
	}

	if (error_code == 0) {
		return -EFAULT;
	} else {
		return error_code;
	}
}

static int ctr_sat_parse_message(uint8_t *response_id, void *response_data,
				 size_t response_data_max_size, size_t *response_data_parsed_len,
				 const void *rx_buf, size_t rx_buf_len, int flags)
{
	int ret;

	ret = ctr_sat_parse_message_container(response_id, rx_buf, rx_buf_len);
	if (ret) {
		LOG_ERR("Call `ctr_sat_parse_message_container` failed: %d", ret);
		return ret;
	}

	if (*response_id == ASTRONODE_S_ANSWER_ERROR) {
		return ctr_sat_parse_error_answer(rx_buf, rx_buf_len, flags);
	}

	ret = ctr_sat_parse_response_data(response_data, response_data_max_size,
					  response_data_parsed_len, rx_buf, rx_buf_len, flags);
	if (ret) {
		LOG_ERR("Call `ctr_sat_parse_response_data` failed: %d", ret);
		return ret;
	}

	ret = ctr_sat_check_message_crc(*response_id, response_data, *response_data_parsed_len,
					rx_buf, rx_buf_len);
	if (ret) {
		LOG_ERR("Call `ctr_sat_check_message_crc` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_sat_encode_message(struct ctr_sat *sat, uint8_t command, const void *parameters,
				  size_t parameters_size)
{
	uint8_t *tx_buf_wrptr = sat->tx_buf;

	/* write start byte */
	*tx_buf_wrptr++ = ASTRONODE_S_START_BYTE;

#define WRITE_HEX_BYTE(b)                                                                          \
	do {                                                                                       \
		uint8_t var = (b);                                                                 \
		size_t written = bin2hex(&var, sizeof(var), tx_buf_wrptr, 3);                      \
		if (written != 2) {                                                                \
			LOG_ERR("Call `bin2hex(0x%02x)` (1) failed: %d", var, written);            \
			return -EFAULT;                                                            \
		}                                                                                  \
		tx_buf_wrptr += written;                                                           \
	} while (0)

	/* write command id */
	WRITE_HEX_BYTE(command);

	/* write parameters */
	size_t written = bin2hex(parameters, parameters_size, tx_buf_wrptr,
				 sat->tx_buf + TX_MESSAGE_MAX_SIZE - tx_buf_wrptr);
	if (written != parameters_size * 2) {
		LOG_ERR("Call `bin2hex` (2) failed: %d", written);
		return -EFAULT;
	}
	tx_buf_wrptr += written;

	/* write CRC */
	uint16_t crc = crc16_itu_t(0xFFFF, &command, sizeof(command));
	crc = crc16_itu_t(crc, parameters, parameters_size);
	WRITE_HEX_BYTE((uint8_t)((crc & 0x00FF) >> 0));
	WRITE_HEX_BYTE((uint8_t)((crc & 0xFF00) >> 8));

	/* write end byte */
	*tx_buf_wrptr++ = ASTRONODE_S_END_BYTE;

	if (tx_buf_wrptr > sat->tx_buf + TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Error while formatting command. Stack possibly corrupted");
		k_oops();
	}

	return 0;
}

int ctr_sat_execute_command(struct ctr_sat *sat, uint8_t command, void *parameters,
			    size_t parameters_size, void *response_data, size_t *response_data_size,
			    int flags)
{
	int ret;

	k_mutex_lock(&sat->mutex, K_FOREVER);

	size_t encoded_command_size = 1 /* start byte */ + 2 /* command ID */ +
				      2 * parameters_size /* command */ + 4 /* CRC */ +
				      1 /* stop byte */;

	size_t encoded_response_size;
	if (response_data_size == NULL) {
		/* 8 == 1 (start byte) + 2 (code) + 4 (crc) + 1 (stop byte) */
		encoded_response_size = 8;
	} else {
		encoded_response_size = 1 /* start byte */ + 2 /* response ID */ +
					2 * (*response_data_size) /* command */ + 4 /* CRC */ +
					1 /* stop byte */;
	}

	if (encoded_command_size > TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded response is too large (%d bytes). Maximum is %d",
			encoded_command_size, TX_MESSAGE_MAX_SIZE);
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	if (encoded_response_size > RX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded response is too large (%d bytes). Maximum is %d",
			encoded_response_size, RX_MESSAGE_MAX_SIZE);
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	ret = ctr_sat_encode_message(sat, command, parameters, parameters_size);
	if (ret) {
		LOG_ERR("Error while formatting command %d", ret);
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	sat->tx_buf_len = encoded_command_size;
	sat->rx_buf_len = 0;

	LOG_DBG("Starting UART transmission. To send: %d bytes. To receive: %d bytes",
		encoded_command_size, encoded_response_size);
	LOG_HEXDUMP_INF(sat->tx_buf, encoded_command_size, "Encoded command to astronode:");

	ret = sat->ctr_sat_uart_write_read(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_uart_write_read` failed: %d", ret);
		k_mutex_unlock(&sat->mutex);
		return ret;
	}

	LOG_DBG("UART transmission completed. Sent: %d bytes. Received: %d bytes",
		encoded_command_size, sat->rx_buf_len);
	LOG_HEXDUMP_INF(sat->rx_buf, sat->rx_buf_len, "Encoded answer from astronode:");

	if (sat->rx_buf_len != encoded_response_size &&
	    FIELD_GET(FLAG_ALLOW_UNEXPECTED_LENGTH, flags) == 0) {
		LOG_WRN("Received unexpected answer with unexpected length. Expected: %d, "
			"Received: %d",
			encoded_response_size, sat->rx_buf_len);
	}

	uint8_t response_id;
	size_t response_data_size_parsed;
	size_t response_data_size_max;
	if (response_data_size == NULL) {
		response_data_size_max = 0;
	} else {
		response_data_size_max = *response_data_size;
	}
	ret = ctr_sat_parse_message(&response_id, response_data, response_data_size_max,
				    &response_data_size_parsed, sat->rx_buf, sat->rx_buf_len,
				    flags);
	if (ret) {
		LOG_ERR("Call `ctr_sat_parse_message` failed: %d", ret);

		int allowed_retries = FIELD_GET(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK, flags);
		int new_flags =
			(flags & ~FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK) |
			FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK, allowed_retries - 1);

		if (ret == ASTRONODE_S_ERROR_CRC_NOT_VALID) {
			if (FIELD_GET(FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT, flags) &&
			    allowed_retries > 0) {
				LOG_DBG("CRC error reply was detected. Trying again");

				k_mutex_unlock(&sat->mutex);
				return ctr_sat_execute_command(sat, command, parameters,
							       parameters_size, response_data,
							       response_data_size, new_flags);
			}
		} else if (ret == -E_INVCRC) {
			if (FIELD_GET(FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT, flags) &&
			    allowed_retries > 0) {
				LOG_DBG("CRC error was detected in answer message. Trying again");

				k_mutex_unlock(&sat->mutex);
				return ctr_sat_execute_command(sat, command, parameters,
							       parameters_size, response_data,
							       response_data_size, new_flags);
			}
		}

		k_mutex_unlock(&sat->mutex);
		return ret;
	}

	uint8_t expected_response_code = ASTRONODE_S_ANSWER_CODE(command);
	if (response_id != expected_response_code) {
		bool log_allowed = FIELD_GET(FLAG_NO_WARN_ASTRONODE_ERROR, flags) == 0;
		bool is_error_ans = response_id == ASTRONODE_S_ANSWER_ERROR;
		if (log_allowed && !is_error_ans) {
			LOG_ERR("Astronode replied with unexpected response. Request code: 0x%02x, "
				"expected "
				"response code: 0x%02x, received response code: 0x%02x",
				command, expected_response_code, response_id);
		}
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	k_mutex_unlock(&sat->mutex);
	return 0;
}
