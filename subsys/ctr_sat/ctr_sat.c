/* CHESTER includes */
#include "astronode_s_messages.h"

#include <chester/ctr_sat.h>
#include <chester/ctr_w1.h>
#include <chester/drivers/w1/ds28e17.h>

/* Zephyr includes */
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

/* Standard includes */
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ctr_sat, CONFIG_CTR_SAT_LOG_LEVEL);

/*
	flags definition as used in ctr_sat_execute_command function. Flags are used only for
   internal pusrpose of this subsys module and are not intended for use by user.

	bit 0: FLAG_NO_WARN_ASTRONODE_ERROR - used when error reply message is not directly error.
   Cleans log from misleading error messages. When this bit is set, ensure to set
   FLAG_ALLOW_UNEXPECTED_LENGTH also becase error messages usualy have different length than
   standard reply.

	bit 1: FLAG_REPEAT_ON_REQ_CRC_ERROR: - allows retrying exectuion of command after receiving
   valid reply containing CRC error message (meaning that CRC error happended when transfering
   request).

	bit 2: FLAG_REPEAT_ON_ANS_CRC_ERROR: - allows retrying exectuion of command exectuion when
   astronode respond with any message and local CRC check fails.

	bit 7:3: FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES: - number of allowed repetition of command
   exectuion when transmission of request or answer is affected by CRC error. For enabling this
   feature you need to specify REPEAT_ON_REQ_CRC_ERROR or REPEAT_ON_ANS_CRC_ERROR or both.

	bit 8: FLAG_ALLOW_UNEXPECTED_LENGTH - used for preventing log warning when received message
   has unexpected length. Cleans log for misleading messages.

	bit 9: FLAG_TRIM_LONG_REPLY - used for commands with TLV structure which can be extended in
   future. This flag allow trimming (future) messages.

   Do not use FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT, FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT and
   FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK macros directly. Instead use
   FLAG_REPEAT_ON_REQ_CRC_ERRORS, FLAG_REPEAT_ON_ANS_CRC_ERRORS or FLAG_REPEAT_ON_ALL_CRC_ERRORS
   which contains embeded both flag and retry count defined by standard Kconfig option.
*/
#define FLAG_NO_WARN_ASTRONODE_ERROR		  BIT(0)
#define FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT	  BIT(1)
#define FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT	  BIT(2)
#define FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK GENMASK(7, 3)
#define FLAG_ALLOW_UNEXPECTED_LENGTH		  BIT(8)
#define FLAG_TRIM_LONG_REPLY			  BIT(9)

#define FLAG_REPEAT_ON_REQ_CRC_ERRORS                                                              \
	(FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT | FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK,  \
						       CONFIG_CTR_SAT_CRC_ERRORS_ALLOWED_RETRIES))

#define FLAG_REPEAT_ON_ANS_CRC_ERRORS                                                              \
	(FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT | FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK,  \
						       CONFIG_CTR_SAT_CRC_ERRORS_ALLOWED_RETRIES))

#define FLAG_REPEAT_ON_ALL_CRC_ERRORS                                                              \
	(FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT | FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT |                     \
	 FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK,                                     \
		    CONFIG_CTR_SAT_CRC_ERRORS_ALLOWED_RETRIES))

/* Invalid CRC error code used for distinguishing from other EIO errors.*/
#define E_INVCRC 10000

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
		LOG_ERR("Call `hex2bin` failed %d", ret);
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
		/* error details are printed in ctr_sat_parse_response_data */
		return ret;
	}

	ret = ctr_sat_check_message_crc(ASTRONODE_S_ANSWER_ERROR, error_answer,
					sizeof(error_answer), rx_buf, rx_buf_len);
	if (ret) {
		/* error details are printed in ctr_sat_check_message_crc */
		return ret;
	}

	uint16_t error_code = sys_get_le16(error_answer);

	if (FIELD_GET(FLAG_NO_WARN_ASTRONODE_ERROR, flags) == 0) {
		LOG_ERR("Astronode command execution failed with error code 0x%04X", error_code);
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
		/* error details are printed in ctr_sat_parse_message_container */
		return ret;
	}

	if (*response_id == ASTRONODE_S_ANSWER_ERROR) {
		return ctr_sat_parse_error_answer(rx_buf, rx_buf_len, flags);
	}

	ret = ctr_sat_parse_response_data(response_data, response_data_max_size,
					  response_data_parsed_len, rx_buf, rx_buf_len, flags);
	if (ret) {
		/* error details are printed in ctr_sat_parse_response_data */
		return ret;
	}

	ret = ctr_sat_check_message_crc(*response_id, response_data, *response_data_parsed_len,
					rx_buf, rx_buf_len);
	if (ret) {
		/* error details are printed in ctr_sat_check_message_crc */
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
			LOG_ERR("Call (1) `bin2hex(0x%02x)` failed %d", var, written);             \
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
		LOG_ERR("Call (2)`bin2hex` failed %d", written);
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

static int ctr_sat_execute_command(struct ctr_sat *sat, uint8_t command, void *parameters,
				   size_t parameters_size, void *response_data,
				   size_t *response_data_size, int flags)
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
		LOG_DBG("Call `ctr_sat_uart_write_read` failed %d.", ret);
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
		/* error details are printed in ctr_sat_parse_message */

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

static int ctr_sat_get_pending_event(struct ctr_sat *sat, int *event)
{
	int ret;

	LOG_DBG("Getting pending events");

	uint8_t response;
	size_t response_size = sizeof(response);
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_EVT_R, NULL, 0, &response,
				      &response_size, FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	*event = (int)response;

	return 0;
}

static int ctr_sat_handle_ack(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Reading satelite ack");

	uint16_t payload_id;
	size_t payload_id_size = sizeof(payload_id);

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_SAK_R, NULL, 0, &payload_id,
				      &payload_id_size, FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` (1) failed %d", ret);
		return ret;
	}

	if (sat->callback) {
		union ctr_sat_event_data data = {
			.msg_send = {.msg = (ctr_sat_msg_handle)payload_id}};
		sat->callback(CTR_SAT_EVENT_MESSAGE_SENT, &data, sat->callback_user_data);
	}

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_SAK_C, NULL, 0, NULL, NULL,
				      FLAG_REPEAT_ON_REQ_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` (2) failed %d", ret);
		return ret;
	}

	LOG_DBG("Sateilite message 0x%04x was processed", payload_id);

	return 0;
}

static int ctr_sat_clear_reset_event(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Clearing reset event");

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_RES_C, NULL, 0, NULL, NULL,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	LOG_DBG("Reset event cleared");

	return 0;
}

static int ctr_sat_reconfigure(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Reading module configuration");

	uint8_t cur_cfg[ASTRONODE_S_CFG_RA_SIZE];
	size_t cur_cfg_size = sizeof(cur_cfg);
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_R, NULL, 0, cur_cfg, &cur_cfg_size,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	LOG_INF("=========== Atronode S configuration ===========");
	if (cur_cfg[ASTRONODE_S_CFG_RA_PRODUCT_ID] == 3) {
		LOG_INF("Product ID: Commercial Satellite Astronode");
	} else if (cur_cfg[ASTRONODE_S_CFG_RA_PRODUCT_ID] == 4) {
		LOG_INF("Product ID: Commercial Wi-Fi Dev Kit");
	} else {
		LOG_INF("Product ID: Unknown");
	}
	LOG_INF("Hardware revision: %d", cur_cfg[ASTRONODE_S_CFG_RA_HW_REV]);
	LOG_INF("Firmware version: %d.%d.%d", cur_cfg[ASTRONODE_S_CFG_RA_FW_VER_MAJOR],
		cur_cfg[ASTRONODE_S_CFG_RA_FW_VER_MINOR],
		cur_cfg[ASTRONODE_S_CFG_RA_FW_VER_REVISION]);
	LOG_INF("");

	LOG_INF("Payload Acknowledgement: %s",
		FIELD_GET(ASTRONODE_S_CFG1_ACK_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("Add Geolocation: %s",
		FIELD_GET(ASTRONODE_S_CFG1_ADD_GEO_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("Ephemeris Enabled: %s",
		FIELD_GET(ASTRONODE_S_CFG1_EPHEMERIS_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("Deep Sleep Mode: %s",
		FIELD_GET(ASTRONODE_S_CFG1_DEEPSLEEP_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG1])
			? "Enabled"
			: "Disabled");

	LOG_INF("");

	LOG_INF("Payload Ack Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_ACK_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("Reset Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_RST_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("Command Avalaible Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_CMD_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("Message TX Pending Event Interrupt: %s",
		FIELD_GET(ASTRONODE_S_CFG3_INT_TX_PENDING_EN_MASK, cur_cfg[ASTRONODE_S_CFG_RA_CFG3])
			? "Enabled"
			: "Disabled");

	LOG_INF("================================================");

	uint8_t new_cfg[ASTRONODE_S_CFG_WR_SIZE];

	new_cfg[ASTRONODE_S_CFG_WR_CFG1] = FIELD_PREP(ASTRONODE_S_CFG1_ACK_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG1_ADD_GEO_MASK, 0) |
					   FIELD_PREP(ASTRONODE_S_CFG1_EPHEMERIS_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG1_DEEPSLEEP_EN_MASK, 0);

	new_cfg[ASTRONODE_S_CFG_WR_CFG2] = 0;

	new_cfg[ASTRONODE_S_CFG_WR_CFG3] = FIELD_PREP(ASTRONODE_S_CFG3_INT_ACK_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG3_INT_RST_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG3_INT_CMD_EN_MASK, 1) |
					   FIELD_PREP(ASTRONODE_S_CFG3_INT_TX_PENDING_EN_MASK, 0);

	LOG_DBG("Updating module configuration");

	if (memcpy(cur_cfg + ASTRONODE_S_CFG_RA_CFG1, new_cfg, sizeof(new_cfg)) != 0) {
		ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_W, new_cfg, sizeof(new_cfg),
					      NULL, NULL, FLAG_REPEAT_ON_ALL_CRC_ERRORS);
		if (ret) {
			LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
			return ret;
		}

		LOG_INF("Configuration was adjusted");
	}

	return 0;
}

#if CONFIG_CTR_SAT_RESET_ON_INIT
static int ctr_sat_reset(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Resseting module");

	ret = sat->ctr_sat_gpio_write(sat, CTR_SAT_PIN_RESET, true);
	if (ret) {
		LOG_ERR("Call `ctr_sat_gpio_write` (1) failed %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = sat->ctr_sat_gpio_write(sat, CTR_SAT_PIN_RESET, false);
	if (ret) {
		LOG_ERR("Call `ctr_sat_gpio_write` (2) failed %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(ASTRONODE_MAX_RESET_WAKEUP_TIME_MSEC + 1));

	return 0;
}
#endif

static int ctr_sat_receive_command(struct ctr_sat *sat)
{
	int ret;

	/* last byte is used for terminating possible string */
	uint8_t cmd_ra[ASTRONODE_S_CMD_RA_SIZE + 1];
	size_t cmd_ra_size = sizeof(cmd_ra) - 1;

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CMD_R, NULL, 0, cmd_ra, &cmd_ra_size,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS | FLAG_ALLOW_UNEXPECTED_LENGTH);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	cmd_ra[cmd_ra_size] = '\0';

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CMD_C, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	if (sat->callback) {
		/* Astrnode date is defined as number of seconds since 2018-01-01 00:00:00 which is
		 * equivalent to unix timestamp 1514764800.*/
		time_t astronode_time_offset = 1514764800UL;

		union ctr_sat_event_data data = {
			.msg_recv = {
				.data = cmd_ra + ASTRONODE_S_CMD_RA_PAYLOAD,
				.data_len = cmd_ra_size - ASTRONODE_S_CMD_RA_PAYLOAD,
				.created_at =
					(time_t)(astronode_time_offset +
						 sys_get_le32(cmd_ra +
							      ASTRONODE_S_CMD_RA_CREATED_DATE))}};
		sat->callback(CTR_SAT_EVENT_MESSAGE_RECEIVED, &data, sat->callback_user_data);
	}

	return 0;
}

static int ctr_sat_handle_event(struct ctr_sat *sat)
{
	int ret;

	int event;
	ret = ctr_sat_get_pending_event(sat, &event);
	if (ret) {
		LOG_DBG("Call `ctr_sat_get_pending_event` failed %d", ret);
		return ret;
	}

	if (event == 0) {
		LOG_DBG("Spurious interrupt detected. Astrnonode module has no event flag set");
		return -EAGAIN;
	}

	LOG_DBG("Astronode event 0x%02x", event);

	if (event & ASTRONODE_S_EVENT_MODULE_RESET) {
		ret = ctr_sat_clear_reset_event(sat);
		if (ret) {
			LOG_WRN("Call `ctr_sat_clear_reset_event` failed %d", ret);
		} else {
			ret = ctr_sat_reconfigure(sat);
			if (ret) {
				LOG_WRN("Call `ctr_sat_reconfigure` failed %d", ret);
			}
		}
	}

	if (event & ASTRONODE_S_EVENT_SAT_ACK) {
		ret = ctr_sat_handle_ack(sat);
		if (ret) {
			LOG_ERR("Call `ctr_sat_handle_ack` failed %d", ret);
		}
	}

	if (event & ASTRONODE_S_EVENT_COMMAND_AVAILABLE) {
		ret = ctr_sat_receive_command(sat);
		if (ret) {
			LOG_ERR("Call `ctr_sat_receive_command` failed %d", ret);
		}
	}

	return 0;
}

static int ctr_sat_poll_intrrupt(struct ctr_sat *sat)
{
	/*
	int ret;

	ret = gpio_pin_get_dt(&sat->module_event_gpio);
	if (ret != 0 && ret != 1) {
		LOG_ERR("Call `gpio_pin_get_dt` failed %d", ret);
		return ret;
	}

	if (ret == 1) {
		return ctr_sat_handle_event(sat);
	} else {
		return 0;
	}
	*/

	return 0;
}

static void ctr_sat_thread_main(struct ctr_sat *sat)
{
	int ret;

	/*
		Thread is responsible for two tasks:
			- Deferred processing of events (blocking tasks triggered by interrupt)
			- Polling interrupt (needed when using over W1)
	*/

	LOG_INF("CTR_SAT thread started");

	while (true) {
		ret = k_sem_take(&sat->event_trig_sem, K_MSEC(CONFIG_CTR_SAT_INT_POLL_PERIOD_MSEC));
		if (ret == 0) {
			ret = ctr_sat_handle_event(sat);
			if (ret) {
				LOG_WRN("Call `ctr_sat_handle_event` failed %d", ret);
			}
		} else if (ret == -EAGAIN) {
			ret = ctr_sat_poll_intrrupt(sat);
			if (ret) {
				LOG_WRN("Call `ctr_sat_poll_intrrupt` failed %d", ret);
			}
		} else {
			LOG_WRN("Call `k_sem_take` failed %d", ret);
			k_sleep(K_MSEC(1));
		}
	}
}

int ctr_sat_init_generic(struct ctr_sat *sat)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	sat->last_payload_id = 0;
	sat->enqued_payloads = 0;

	sat->callback = NULL;
	sat->callback_user_data = NULL;

	k_mutex_init(&sat->mutex);
	k_sem_init(&sat->event_trig_sem, 0, K_SEM_MAX_LIMIT);

#if CONFIG_CTR_SAT_RESET_ON_INIT
	ret = ctr_sat_reset(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_reset` failed %d", ret);
		return ret;
	}
#endif

	ret = ctr_sat_reconfigure(sat);
	if (ret) {
		LOG_DBG("Call `ctr_sat_reconfigure` failed %d", ret);
		return ret;
	}

	return 0;
}

int ctr_sat_start(struct ctr_sat *sat)
{
	sat->thread_id =
		k_thread_create(&sat->thread, sat->thread_stack, CONFIG_CTR_SAT_THREAD_STACK_SIZE,
				(k_thread_entry_t)ctr_sat_thread_main, sat, NULL, NULL,
				K_PRIO_COOP(CONFIG_CTR_SAT_THREAD_PRIORITY), 0, K_NO_WAIT);

	return 0;
}

int ctr_sat_set_callback(struct ctr_sat *sat, ctr_sat_event_cb user_cb, void *user_data)
{
	sat->callback = user_cb;
	sat->callback_user_data = user_data;

	return 0;
}

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
int ctr_sat_use_wifi_devkit(struct ctr_sat *sat, const char *ssid, const char *password,
			    const char *api_key)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	ssize_t ssid_len = strlen(ssid);
	if (ssid_len < 1 || ssid_len > (ASTRONODE_S_WFI_WR_SSID_SIZE - 1)) {
		LOG_ERR("Wi-Fi SSID length exceeded maximum allowed length %d",
			ASTRONODE_S_WFI_WR_SSID_SIZE - 1);
		return -EINVAL;
	}

	ssize_t password_len = strlen(password);
	if (password_len < 1 || password_len > (ASTRONODE_S_WFI_WR_PASSWORD_SIZE - 1)) {
		LOG_ERR("Wi-Fi password length exceeded maxmimum allowed length %d",
			ASTRONODE_S_WFI_WR_PASSWORD_SIZE - 1);
		return -EINVAL;
	}

	ssize_t api_key_len = strlen(api_key);
	if (api_key_len != (ASTRONODE_S_WFI_WR_API_KEY_SIZE - 1)) {
		LOG_ERR("API key must be exactly 96 characters. You passed string of length %d",
			api_key_len);
		return -EINVAL;
	}

	uint8_t req[ASTRONODE_S_WFI_WR_SIZE] = {0};
	memcpy(req + ASTRONODE_S_WFI_WR_SSID, ssid, ssid_len);
	memcpy(req + ASTRONODE_S_WFI_WR_PASSWORD, password, password_len);
	memcpy(req + ASTRONODE_S_WFI_WR_API_KEY, api_key, api_key_len);

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_WIF_W, req, sizeof(req), NULL, NULL,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	return 0;
}
#endif

static uint16_t ctr_sat_get_next_payload_id(struct ctr_sat *sat)
{
	uint16_t next_id = sat->last_payload_id + 1;
	if (next_id == 0xFFFF) {
		next_id = 0;
	}

	sat->last_payload_id = next_id;

	return next_id;
}

int ctr_sat_send_message(struct ctr_sat *sat, ctr_sat_msg_handle *msg_handle, const void *buf,
			 const size_t len)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	if (sat->enqued_payloads == MAX_PAYLOADS) {
		LOG_ERR("Unable to send message because end buffer is full");
		return -EAGAIN;
	}

	if (atomic_get(&sat->is_started) == 0) {
		LOG_ERR("satelite module is not started. Please call ctr_sat_start() "
			"first");
		ret = -EACCES;
		goto exit;
	}

	if (len > ASTRONODE_S_PLD_ER_DATA_SIZE) {
		LOG_DBG("Message is too long. Maximum allowed length is %d",
			ASTRONODE_S_PLD_ER_DATA_SIZE);
		ret = -EINVAL;
		goto exit;
	}

	uint16_t payload_id = ctr_sat_get_next_payload_id(sat);

	uint8_t req[ASTRONODE_S_PLD_ER_SIZE] = {0};
	sys_put_le16(payload_id, req + ASTRONODE_S_PLD_ER_ID);
	memcpy(req + ASTRONODE_S_PLD_ER_DATA, buf, len);

	bool was_previous_answer_affected_by_crc_error = false;
	do {
		uint8_t ans[ASTRONODE_S_PLD_EA_SIZE];
		size_t ans_size = sizeof(ans);

		ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_E, req, 2 + len, &ans,
					      &ans_size, 0);
		if (ret) {
			if (ret == ASTRONODE_S_ERROR_DUPLICATE_ID) {
				if (was_previous_answer_affected_by_crc_error) {
					/* dumplicate ID here means that previous request was
					 * successfull and only answer was malformed */
					ret = 0;
					goto exit;
				} else {
					was_previous_answer_affected_by_crc_error = false;
					LOG_INF("Message with id 0x%04x was already enqueud. "
						"Retrying with different ID",
						payload_id);
					payload_id = ctr_sat_get_next_payload_id(sat);
					sys_put_le16(payload_id, req + ASTRONODE_S_PLD_ER_ID);
				}
			} else if (ret == ASTRONODE_S_ERROR_CRC_NOT_VALID) {
				was_previous_answer_affected_by_crc_error = false;
				LOG_INF("Enquing message with id 0x%04x failed because of CRC "
					"error on request transmission. Retrying with the same "
					"ID",
					payload_id);
			} else if (ret == -E_INVCRC) {
				was_previous_answer_affected_by_crc_error = true;
				LOG_INF("Enquing message with id 0x%04x failed because of CRC "
					"error on answer transmission. Retrying with the same ID",
					payload_id);
			} else {
				was_previous_answer_affected_by_crc_error = false;
				LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
				goto exit;
			}
		}
	} while (ret == ASTRONODE_S_ERROR_DUPLICATE_ID || ret == ASTRONODE_S_ERROR_CRC_NOT_VALID);

	if (msg_handle) {
		*msg_handle = payload_id;
	}

exit:
	return ret;
}

int ctr_sat_flush_messages(struct ctr_sat *sat)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	LOG_DBG("Flushing all messages");

	/* ASTRONODE_S_ERROR_BUFFER_EMPTY is allowed error */
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_F, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_ALL_CRC_ERRORS | FLAG_NO_WARN_ASTRONODE_ERROR |
					      FLAG_ALLOW_UNEXPECTED_LENGTH);
	if (ret) {
		if (ret < 0) {
			LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
			return ret;
		} else if (ret != ASTRONODE_S_ERROR_BUFFER_EMPTY) {
			LOG_WRN("Satellite module error 0x%04x", ret);
			return -EIO;
		}
	}

	return 0;
}

int ctr_sat_stop(struct ctr_sat *sat)
{
	atomic_set(&(sat->is_started), false);
	return 0;
}
