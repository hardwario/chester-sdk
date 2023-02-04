/* CHESTER includes */
#include "astronode_s_messages.h"

#include <chester/ctr_sat.h>

/* Zephyr includes */
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
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
   internal pusrpose of this subsys and are not intended for use by user.

	bit 0: NO_WARN_ASTRONODE_ERROR - used when error reply message is not directly error. Cleans
   log for misleading error messages. When this bit is set, ensure to set
   FLAG_ALLOW_UNEXPECTED_LENGTH also becase error messages ususaly have different length than
   standard reply.

	bit 3:1: REPEAT_ON_CRC_ERROR: - number of allowed repetition of command exectuioon when
   astronode respod with CRC error.

	bit 4: FLAG_ALLOW_UNEXPECTED_LENGTH - used for preventing log warning when received message
   has unexpected length. Cleans log for misleading messages.

	bit 5: FLAG_TRIM_LONG_REPLY - used for commands with TLV structure which can be extended in
   future. This flag allow trimming (future) messages.
*/
#define FLAG_NO_WARN_ASTRONODE_ERROR  BIT(0)
#define FLAG_REPEAT_ON_CRC_ERROR_MASK GENMASK(3, 1)
#define FLAG_REPEAT_ON_CRC_ERROR                                                                   \
	FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MASK, CONFIG_CTR_SAT_CRC_ERRORS_ALLOWED_RETRIES)
#define FLAG_ALLOW_UNEXPECTED_LENGTH BIT(4)
#define FLAG_TRIM_LONG_REPLY	     BIT(5)

#define E_INVCRC 10000

// TODO comment describing fce
static int ctr_sat_check_message_crc(uint8_t response_id, uint8_t *parameters,
				     size_t parameters_len, uint8_t *rx_buf, size_t rx_buf_len)
{
	int ret;
	uint16_t actual_crc = crc16_itu_t(0xFFFF, &response_id, sizeof(response_id));
	actual_crc = crc16_itu_t(actual_crc, parameters, parameters_len);

	char crc_bytes[2];

	ret = hex2bin(rx_buf + rx_buf_len - 5, 4, crc_bytes, sizeof(crc_bytes));
	if (ret != sizeof(crc_bytes)) {
		LOG_ERR("Error while parsing response message. Mallformed CRC byte value.");
		return -E_INVCRC; // ret == 0 when hex2bin fail
	}

	uint16_t message_crc = (((uint16_t)crc_bytes[1]) << 8) | (uint16_t)crc_bytes[0];

	if (message_crc != actual_crc) {
		LOG_ERR("CRC mismatch detected when checking CRC of response message. Expected "
			"CRC: %04X. Actual CRC: %04X",
			message_crc, actual_crc);
		return -E_INVCRC;
	}

	return 0;
}

// TODO comment describing fce
static int ctr_sat_parse_message_container(uint8_t *response_id, uint8_t *rx_buf, size_t rx_buf_len)
{
	int ret;

	if (rx_buf_len < 8) { /* 8 == 1 (start byte) + 2 (code) + 4 (crc) + 1 (stop byte) */
		LOG_ERR("Error while parsing response message. Message too short.");
		return -EINVAL;
	}

	if (rx_buf[0] != ASTRONODE_S_START_BYTE) {
		LOG_ERR("Error while parsing response message. Unexpected start of message.");
		return -EINVAL;
	}

	if (rx_buf[rx_buf_len - 1] != ASTRONODE_S_END_BYTE) {
		LOG_ERR("Error while parsing response message. Missing end of message.");
		return -EINVAL;
	}

	ret = hex2bin((const char *)(rx_buf + 1), 2, (char *)response_id, sizeof(*response_id));
	if (ret != sizeof(*response_id)) {
		LOG_ERR("Error while parsing response message. Mallformed reply ID byte received. "
			"ret=%d",
			ret);
		return -EIO; // ret == 0 when hex2bin fail
	}

	return 0;
}

// TODO comment describing fce
static int ctr_sat_parse_response_data(uint8_t *response_data, size_t response_data_size,
				       uint8_t *rx_buf, size_t rx_buf_len)
{
	int ret;

	// check message length
	if (8 + response_data_size * 2 !=
	    rx_buf_len) { /* 8 == 1 (start byte) + 2 (code) + 4 (crc) + 1 (stop byte) */
		LOG_ERR("Received asnwer with invalid length. Expected: %d, Received: %d",
			8 + response_data_size * 2, rx_buf_len);
		return -EINVAL;
	}

	ret = hex2bin(rx_buf + 3, MIN(rx_buf_len - 3, response_data_size * 2), response_data,
		      response_data_size);
	if (ret != response_data_size) {
		LOG_ERR("Error while parsing response message ret=%d", ret);
		return -EIO; // ret == 0 when hex2bin fail
	}

	return 0;
}

// TODO comment describing fce
static int ctr_sat_parse_error_answer(uint8_t *rx_buf, size_t rx_buf_len, int flags)
{
	int ret;

	astronode_error_answer error_answer;
	ret = ctr_sat_parse_response_data((uint8_t *)&error_answer, sizeof(error_answer), rx_buf,
					  rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_parse_response_data
		return ret;
	}

	ret = ctr_sat_check_message_crc(ASTRONODE_S_ANSWER_ERROR, (uint8_t *)&error_answer,
					sizeof(error_answer), rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_check_message_crc
		return ret;
	}

	if (FIELD_GET(FLAG_NO_WARN_ASTRONODE_ERROR, flags) == 0) {
		LOG_ERR("Astronode command execution failed with error code 0x%04X.",
			error_answer.error_code);
	}

	if (error_answer.error_code == 0) {
		return -EFAULT;
	} else {
		return error_answer.error_code;
	}
}

// TODO comment describing fce
static int ctr_sat_parse_message(uint8_t *response_id, void *response_data,
				 size_t response_data_size, void *rx_buf, size_t rx_buf_len,
				 int flags)
{
	int ret;

	ret = ctr_sat_parse_message_container(response_id, rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_parse_message_container
		return ret;
	}

	if (*response_id == ASTRONODE_S_ANSWER_ERROR) {
		return ctr_sat_parse_error_answer(rx_buf, rx_buf_len, flags);
	}

	ret = ctr_sat_parse_response_data(response_data, response_data_size, rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_parse_response_data
		return ret;
	}

	ret = ctr_sat_check_message_crc(*response_id, response_data, response_data_size, rx_buf,
					rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_check_message_crc
		return ret;
	}

	return 0;
}

static int ctr_sat_encode_message(struct ctr_sat *sat, uint8_t command, void *parameters,
				  size_t parameters_size)
{
	uint8_t *tx_buf_wrptr = sat->tx_buf;

	// write start byte
	*tx_buf_wrptr++ = ASTRONODE_S_START_BYTE;

#define WRITE_HEX_BYTE(b)                                                                          \
	do {                                                                                       \
		uint8_t var = (b);                                                                 \
		size_t written = bin2hex(&var, sizeof(var), tx_buf_wrptr, 3);                      \
		if (written != 2) {                                                                \
			LOG_ERR("Call (1)`bin2hex(0x%02x)` failed ret=%d.", var, written);         \
			return -EFAULT;                                                            \
		}                                                                                  \
		tx_buf_wrptr += written;                                                           \
	} while (0)

	// write command id
	WRITE_HEX_BYTE(command);

	// write parameters
	size_t written = bin2hex(parameters, parameters_size, tx_buf_wrptr,
				 sat->tx_buf + TX_MESSAGE_MAX_SIZE - tx_buf_wrptr);
	if (written != parameters_size * 2) {
		LOG_ERR("Call (2)`bin2hex` failed ret=%d.", written);
		return -EFAULT;
	}
	tx_buf_wrptr += written;

	// write CRC
	uint16_t crc = crc16_itu_t(0xFFFF, &command, sizeof(command));
	crc = crc16_itu_t(crc, parameters, parameters_size);
	WRITE_HEX_BYTE((uint8_t)((crc & 0x00FF) >> 0));
	WRITE_HEX_BYTE((uint8_t)((crc & 0xFF00) >> 8));

	// write end byte
	*tx_buf_wrptr++ = ASTRONODE_S_END_BYTE;

	if (tx_buf_wrptr > sat->tx_buf + TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Error while formatting command. Stack possibly corrupted.");
		k_oops();
	}

	return 0;
}

// TODO comment describing fce
static int ctr_sat_execute_command(struct ctr_sat *sat, uint8_t command, void *parameters,
				   size_t parameters_size, void *response_data,
				   size_t response_data_size, int flags)
{
	int ret;

	k_mutex_lock(&sat->mutex, K_FOREVER);

	size_t encoded_command_size = 1 /* start byte */ + 2 /* command ID */ +
				      2 * parameters_size /* command */ + 4 /* CRC */ +
				      1 /* stop byte */;

	size_t encoded_response_size = 1 /* start byte */ + 2 /* response ID */ +
				       2 * response_data_size /* command */ + 4 /* CRC */ +
				       1 /* stop byte */;

	if (encoded_command_size > TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded command is too large.");
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	if (encoded_response_size > RX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded response is too large.");
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	ret = ctr_sat_encode_message(sat, command, parameters, parameters_size);
	if (ret) {
		LOG_ERR("Error while formatting command %d.", ret);
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	sat->tx_buf_transmit_ptr = sat->tx_buf;
	sat->tx_buf_len = encoded_command_size;

	sat->rx_buf_receive_ptr = sat->rx_buf;
	sat->rx_buf_len = 0;

	LOG_DBG("Starting UART transmission. To send: %d bytes. To receive: %d bytes.",
		encoded_command_size, encoded_response_size);
	LOG_HEXDUMP_INF(sat->tx_buf, encoded_command_size, "Encoded command to astronode:");

	k_poll_signal_reset(&sat->rx_completed_signal);

	uart_irq_rx_enable(sat->uart_dev);
	uart_irq_tx_enable(sat->uart_dev);

	// wait for reply
	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY,
					 &sat->rx_completed_signal),
	};

	// TODO: Check that timeout matches astronode specification. Take in account
	// delays of transsmision and recpetion delays due to complicated chip
	// structure (I2C, 1W and UART bridges)
	// Following timeout must also take into account delay of command transmission. Astronode
	// UART is only 9600 bauds.
	ret = k_poll(events, ARRAY_SIZE(events),
		     K_MSEC(100 + 1000 * encoded_command_size * 10 / 9600));
	if (ret == -EAGAIN) {
		LOG_ERR("Command failed because Astronode module did not respond within specified "
			"time. This may happen when module is disconnected");
		k_mutex_unlock(&sat->mutex);
		return ret;
	}
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		k_mutex_unlock(&sat->mutex);
		return ret;
	}

	LOG_DBG("UART transmission completed. Sent: %d bytes. Received: %d bytes.",
		encoded_command_size, sat->rx_buf_len);
	LOG_HEXDUMP_INF(sat->rx_buf, sat->rx_buf_len, "Encoded answer from astronode:");

	if (sat->rx_buf_len != encoded_response_size &&
	    FIELD_GET(FLAG_ALLOW_UNEXPECTED_LENGTH, flags) == 0) {
		LOG_WRN("Received unexpected answer with unexpected length. Expected: %d, "
			"Received: %d",
			encoded_response_size, sat->rx_buf_len);
	}

	uint8_t response_id;
	ret = ctr_sat_parse_message(&response_id, response_data, response_data_size, sat->rx_buf,
				    sat->rx_buf_len, flags);
	if (ret) {
		// error details printed in ctr_sat_parse_message

		if (ret == ASTRONODE_S_ERROR_CRC_NOT_VALID || ret == -E_INVCRC) {
			int allowed_retries = FIELD_GET(FLAG_REPEAT_ON_CRC_ERROR_MASK, flags);
			if (allowed_retries > 0) {

				LOG_DBG("CRC error reply was detected. Trying again");

				int new_flags = flags & ~FLAG_REPEAT_ON_CRC_ERROR_MASK;
				new_flags |= FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MASK,
							allowed_retries - 1);

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
		if (!(response_id == ASTRONODE_S_ANSWER_ERROR &&
		      FIELD_GET(FLAG_NO_WARN_ASTRONODE_ERROR, flags) == 0)) {
		}

		LOG_ERR("Astronode replied with unexpected response. Request code: %02X, "
			"expected "
			"response code: %02X, received response code: %02X",
			command, expected_response_code, response_id);
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
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_EVT_R, NULL, 0, &response,
				      sizeof(response), FLAG_REPEAT_ON_CRC_ERROR);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	*event = (int)response;

	return 0;
}

static int ctr_sat_read_ack(struct ctr_sat *sat, uint16_t *payload_id)
{
	int ret;

	LOG_DBG("Reading satelite ack");

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_SAK_R, NULL, 0, payload_id,
				      sizeof(*payload_id), FLAG_REPEAT_ON_CRC_ERROR);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	return 0;
}

static int ctr_sat_clear_reset_event(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Clearing reset event");

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_RES_C, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_CRC_ERROR);
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

	uint8_t cur_cfg[8];
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_R, NULL, 0, cur_cfg, sizeof(cur_cfg),
				      FLAG_REPEAT_ON_CRC_ERROR);
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

	uint8_t new_cfg[3];

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

	if (memcpy(cur_cfg + ASTRONODE_S_CFG_RA_CFG1, new_cfg, 3) != 0) {
		ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_W, new_cfg, sizeof(new_cfg),
					      NULL, 0, FLAG_REPEAT_ON_CRC_ERROR);
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

	ret = gpio_pin_set_dt(&sat->module_reset_gpio, 1);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = gpio_pin_set_dt(&sat->module_reset_gpio, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(ASTRONODE_MAX_RESET_WAKEUP_TIME_MSEC + 1));

	return 0;
}
#endif

static void ctr_sat_handle_uart_rx_irq(struct ctr_sat *sat)
{
	int ret;

	// TODO remove MIN()
	// MIN() is here only because underlaying driver (sc16is7xx) currently returns err when
	// passed buffer is too large
	int bytes_read = uart_fifo_read(sat->uart_dev, sat->rx_buf_receive_ptr,
					MIN(RX_MESSAGE_MAX_SIZE - sat->rx_buf_len, 64));

	if (bytes_read < 0) {
		LOG_ERR("Call `uart_fifo_read` failed %d", bytes_read);
		return;
	}

	if (bytes_read == 0 && RX_MESSAGE_MAX_SIZE - sat->rx_buf_len != 0) {
		LOG_WRN("Call `uart_fifo_read` read 0 bytes");
	}

	bool contains_stop_byte = false;

	for (int i = 0; i < bytes_read; i++) {
		if (sat->rx_buf_receive_ptr[i] == ASTRONODE_S_END_BYTE) {
			contains_stop_byte = true;
			// stop byte should be last received byte
			if (i != bytes_read - 1) {
				LOG_WRN("Stop byte was received but satelite module send "
					"additional data after stop byte!");
			}
			break;
		}
	}

	sat->rx_buf_receive_ptr += bytes_read;
	sat->rx_buf_len += bytes_read;

	LOG_DBG("Received %d bytes from satelite module. stop_byte=%d, "
		"remaining_buf_size=%d",
		bytes_read, contains_stop_byte, RX_MESSAGE_MAX_SIZE - sat->rx_buf_len);

	if (RX_MESSAGE_MAX_SIZE - sat->rx_buf_len == 0 || contains_stop_byte) {
		int status;

		if (contains_stop_byte) {
			status = 0;
		} else {
			status = -ENOSPC;
		}

		ret = k_poll_signal_raise(&sat->rx_completed_signal, status);
		if (ret < 0) {
			LOG_ERR("Call `k_poll_signal_raise` failed with status %d!", ret);
			k_oops();
		}

		LOG_DBG("Disabling RX interrupt.");

		uart_irq_rx_disable(sat->uart_dev);
	}
}

static void ctr_sat_handle_tx_irq(struct ctr_sat *sat)
{
	if (sat->tx_buf_len == 0) {
		LOG_DBG("TX completed. No bytes remains to send. Disabling TX interrupt.");
		uart_irq_tx_disable(sat->uart_dev);
		return;
	}

	// TODO remove MIN()
	// MIN() is here only because underlaying driver (sc16is7xx) currently returns err when
	// passed buffer is too large
	int bytes_sent =
		uart_fifo_fill(sat->uart_dev, sat->tx_buf_transmit_ptr, MIN(sat->tx_buf_len, 64));
	if (bytes_sent < 0) {
		LOG_ERR("Call `uart_fifo_fill` failed: %d", bytes_sent);
		return;
	}

	sat->tx_buf_transmit_ptr += bytes_sent;
	sat->tx_buf_len -= bytes_sent;

	if (bytes_sent == 0) {
		LOG_ERR("Call `uart_fifo_fill` accepted 0 bytes!");
	} else {
		LOG_DBG("Written %u byte(s) to FIFO. %u bytes remains.", bytes_sent,
			sat->tx_buf_len);
	}
}

static void ctr_sat_uart_callback(const struct device *dev, void *user_data)
{
	struct ctr_sat *sat = (struct ctr_sat *)user_data;

	if (!uart_irq_update(sat->uart_dev)) {
		LOG_WRN("uart_irq_update did not allow execution of UART interrupt handler. "
			"Possible spurious interrupt.");
		return;
	}

	int isHandled = 0;

	if (uart_irq_rx_ready(sat->uart_dev)) {
		isHandled = 1;
		ctr_sat_handle_uart_rx_irq(sat);
	}

	if (uart_irq_tx_ready(sat->uart_dev)) {
		isHandled = 1;
		ctr_sat_handle_tx_irq(sat);
	}

	if (!isHandled) {
		LOG_WRN("Possible spurious UART interrupt. No TX and no RX flag was set.");
	}
}

static void ctr_sat_event_gpio_callback(const struct device *dev, struct gpio_callback *cb,
					uint32_t pin_mask)
{
	struct ctr_sat *sat = CONTAINER_OF(cb, struct ctr_sat, event_cb);
	k_sem_give(&sat->event_trig_sem);
}

static int ctr_sat_receive_command(struct ctr_sat *sat)
{
	int ret;

	// last byte is used for terminating possible string
	uint8_t cmd_ra[41];

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CMD_R, NULL, 0, cmd_ra, sizeof(cmd_ra),
				      FLAG_REPEAT_ON_CRC_ERROR | FLAG_ALLOW_UNEXPECTED_LENGTH);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CMD_C, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_CRC_ERROR);
	if (ret) {
		LOG_ERR("Call `ctr_sat_execute_command` failed %d", ret);
		return ret;
	}

	if (sat->callback) {
		union ctr_sat_event_data data = {.msg_recv = {.data = cmd_ra + 4, .data_len = 0}};
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
		LOG_DBG("Spurious interrupt detected. Astrnonode module has no event flag set.");
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
		uint16_t confirmed_message;

		ret = ctr_sat_read_ack(sat, &confirmed_message);
		if (ret) {
			LOG_ERR("Call `ctr_sat_read_ack` failed %d", ret);
		} else {
			if (sat->callback) {
				union ctr_sat_event_data data = {
					.msg_send = {.msg = (message_handle)confirmed_message}};
				sat->callback(CTR_SAT_EVENT_MESSAGE_SENT, &data,
					      sat->callback_user_data);
			}
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
	int ret;

	ret = gpio_pin_get_dt(&sat->module_reset_gpio);
	if (ret != 0 && ret != 1) {
		LOG_ERR("Call `gpio_pin_get_dt` failed %d", ret);
		return ret;
	}

	if (ret == 1) {
		return ctr_sat_handle_event(sat);
	} else {
		return 0;
	}
}

static void ctr_sat_thread_main(struct ctr_sat *sat)
{
	int ret;

	/*
		Thread is responsible for two tasks:
			- Deferred processing of events (blocking tasks triggered by interrupt)
			- Polling interrupt (needed when using over W1 and for handling edge cases
	   of level interrupt detection using edge interrupts)
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

int ctr_sat_start(struct ctr_sat *sat, const struct ctr_sat_hwcfg *hwcfg)
{
	int ret;

	if (!device_is_ready(hwcfg->uart_dev)) {
		LOG_ERR("UART Device not ready");
		k_oops();
	}

	sat->uart_dev = hwcfg->uart_dev;

	memcpy(&sat->module_reset_gpio, &hwcfg->module_reset_gpio, sizeof(sat->module_reset_gpio));
	memcpy(&sat->module_wakeup_gpio, &hwcfg->module_wakeup_gpio,
	       sizeof(sat->module_wakeup_gpio));
	memcpy(&sat->module_event_gpio, &hwcfg->module_event_gpio, sizeof(sat->module_event_gpio));

	sat->last_payload_id = 0;
	sat->enqued_payloads = 0;

	sat->callback = NULL;
	sat->callback_user_data = NULL;

	k_mutex_init(&sat->mutex);
	k_poll_signal_init(&sat->rx_completed_signal);

	k_sem_init(&sat->event_trig_sem, 0, K_SEM_MAX_LIMIT);

	uart_irq_callback_user_data_set(sat->uart_dev, ctr_sat_uart_callback, sat);

	ret = gpio_pin_configure_dt(&sat->module_reset_gpio, GPIO_OUTPUT | GPIO_ACTIVE_HIGH);
	if (ret) {
		LOG_DBG("Call `gpio_pin_configure_dt` failed %d", ret);
		return ret;
	}

	ret = gpio_pin_set_dt(&sat->module_reset_gpio, 0);
	if (ret) {
		LOG_ERR("Call `gpio_pin_set_dt` failed %d", ret);
		return ret;
	}

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

	gpio_init_callback(&sat->event_cb, ctr_sat_event_gpio_callback,
			   BIT(sat->module_event_gpio.pin));

	ret = gpio_add_callback(sat->module_event_gpio.port, &sat->event_cb);
	if (ret) {
		LOG_ERR("Call `gpio_add_callback` failed %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&sat->module_event_gpio, GPIO_INT_EDGE_RISING);
	if (ret) {
		LOG_ERR("Call `gpio_pin_interrupt_configure_dt` failed %d", ret);
		return ret;
	}

	k_thread_create(&sat->thread, sat->thread_stack, CONFIG_CTR_SAT_THREAD_STACK_SIZE,
			(k_thread_entry_t)ctr_sat_thread_main, sat, NULL, NULL,
			K_PRIO_COOP(CONFIG_CTR_SAT_THREAD_PRIORITY), 0, K_NO_WAIT);

	return 0;
}

int ctr_sat_set_callback(struct ctr_sat *sat, ctr_sat_message_event_cb user_cb, void *user_data)
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

	astronode_wfi_w_request req = {0};

	ret = snprintf(req.ssid, sizeof(req.ssid), "%s", ssid);
	if (ret < 0) {
		LOG_ERR("Call `snprintf` failed: %d", ret);
		k_oops();

	} else if (ret >= sizeof(req.ssid)) {
		LOG_ERR("Wi-Fi SSID length exceeded maximum allowed length.");
		return -EINVAL;
	}

	ret = snprintf(req.password, sizeof(req.password), "%s", password);
	if (ret < 0) {
		LOG_ERR("snprintf failed with return code %d", ret);
		k_oops();
	}
	if (ret >= sizeof(req.password)) {
		LOG_ERR("Wi-Fi password length exceeded maxmimum allowed length.");
		return -EINVAL;
	}

	ret = snprintf(req.api_key, sizeof(req.api_key), "%s", api_key);
	if (ret < 0) {
		LOG_ERR("snprintf failed with return code %d", ret);
		return ret;

	} else if (ret != 96) {
		LOG_ERR("API key must be exactly 96 characters. You passed string of length %d.",
			ret);
		return -EINVAL;
	}

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_WIF_W, &req, sizeof(req), NULL, 0,
				      FLAG_REPEAT_ON_CRC_ERROR);
	if (ret) {
		LOG_ERR("Error while executing WIF_W command. Error code: %d", ret);
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

int ctr_sat_send_message(struct ctr_sat *sat, message_handle *msg_handle, const void *buf,
			 size_t len)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	if (sat->enqued_payloads == MAX_PAYLOADS) {
		LOG_ERR("Unable to send message because end buffer is full.");
		return -EAGAIN;
	}

	if (atomic_get(&sat->is_started) == 0) {
		LOG_ERR("satelite module is not started. Please call ctr_sat_start() "
			"first.");
		ret = -EACCES;
		goto exit;
	}

	if (len > ASTRONODE_MAX_MESSAGE_LEN) {
		LOG_DBG("Message is too long.");
		ret = -EINVAL;
		goto exit;
	}

	astronode_pld_e_request req = {.id = ctr_sat_get_next_payload_id(sat)};
	memcpy(req.message, buf, len);

	do {
		astronode_pld_e_answer ans = {0};
		ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_E, (uint8_t *)&req, 2 + len,
					      (uint8_t *)&ans, sizeof(ans),
					      FLAG_REPEAT_ON_CRC_ERROR);
		if (ret) {
			if (ret == ASTRONODE_S_ERROR_DUPLICATE_ID) {
				LOG_INF("Message with id 0x%04x was already enqueud. Retrying with "
					"different ID.",
					req.id);
				req.id = ctr_sat_get_next_payload_id(sat);
			} else {
				LOG_ERR("Error while executing PLD_E command. Error code: %d", ret);
				goto exit;
			}
		}
	} while (ret == ASTRONODE_S_ERROR_DUPLICATE_ID);

	if (msg_handle) {
		*msg_handle = req.id;
	}

exit:
	return ret;
}

int ctr_sat_flush_message(struct ctr_sat *sat, message_handle msg_handle)
{
	return -9999;
}

int ctr_sat_flush_messages(struct ctr_sat *sat)
{
	int ret;

	LOG_DBG("Flushing all messages");

	// ASTRONODE_S_ERROR_BUFFER_EMPTY is allowed error
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_F, NULL, 0, NULL, 0,
				      FLAG_REPEAT_ON_CRC_ERROR | FLAG_NO_WARN_ASTRONODE_ERROR |
					      FLAG_ALLOW_UNEXPECTED_LENGTH);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		return ret;
	} else if (ret) {
		if (ret != ASTRONODE_S_ERROR_BUFFER_EMPTY) {
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
