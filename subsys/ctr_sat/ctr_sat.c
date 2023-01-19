
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
		return -EIO; // ret == 0 when hex2bin fail
	}

	uint16_t message_crc = (((uint16_t)crc_bytes[1]) << 8) | (uint16_t)crc_bytes[0];

	if (message_crc != actual_crc) {
		LOG_ERR("CRC mismatch detected when checking CRC of response message. Expected "
			"CRC: %04X. Actual CRC: %04X",
			message_crc, actual_crc);
		return -EIO;
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
static int ctr_sat_parse_error_answer(uint8_t *rx_buf, size_t rx_buf_len)
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

	LOG_ERR("Astronode command execution failed with error code 0x%04X.",
		error_answer.error_code);

	if (error_answer.error_code == 0) {
		return -EFAULT;
	} else {
		return error_answer.error_code;
	}
}

// TODO comment describing fce
static int ctr_sat_parse_message(uint8_t *response_id, void *response_data,
				 size_t response_data_size, void *rx_buf, size_t rx_buf_len)
{
	int ret;

	ret = ctr_sat_parse_message_container(response_id, rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_parse_message_container
		return ret;
	}

	if (*response_id == ASTRONODE_S_ANSWER_ERROR) {
		return ctr_sat_parse_error_answer(rx_buf, rx_buf_len);
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
				   size_t response_data_size)
{
	int ret;

	size_t encoded_command_size = 1 /* start byte */ + 2 /* command ID */ +
				      2 * parameters_size /* command */ + 4 /* CRC */ +
				      1 /* stop byte */;

	size_t encoded_response_size = 1 /* start byte */ + 2 /* response ID */ +
				       2 * response_data_size /* command */ + 4 /* CRC */ +
				       1 /* stop byte */;

	if (encoded_command_size > TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded command is too large.");
		return -EINVAL;
	}

	if (encoded_response_size > RX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded response is too large.");
		return -EINVAL;
	}

	ret = ctr_sat_encode_message(sat, command, parameters, parameters_size);
	if (ret) {
		LOG_ERR("Error while formatting command %d.", ret);
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
			"time. This may happen when module is disconnected.");
		return ret;
	}
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	LOG_DBG("UART transmission completed. Sent: %d bytes. Received: %d bytes.",
		encoded_command_size, sat->rx_buf_len);
	LOG_HEXDUMP_INF(sat->rx_buf, sat->rx_buf_len, "Encoded answer from astronode:");

	if (sat->rx_buf_len != encoded_response_size) {
		LOG_WRN("Received unexpected answer with unexpected length. Expected: %d, "
			"Received: %d",
			encoded_response_size, sat->rx_buf_len);
	}

	uint8_t response_id;
	ret = ctr_sat_parse_message(&response_id, response_data, response_data_size, sat->rx_buf,
				    sat->rx_buf_len);
	if (ret) {
		// error details printed in ctr_sat_parse_message
		return ret;
	}

	uint8_t expected_response_code = ASTRONODE_S_ANSWER_CODE(command);
	if (response_id != expected_response_code) {
		LOG_ERR("Astronode replied with unexpected response. Request code: %02X, expected "
			"response code: %02X, received response code: %02X",
			command, expected_response_code, response_id);
		return -EINVAL;
	}

	return 0;
}

static void ctr_sat_handle_uart_rx_irq(struct ctr_sat *sat)
{
	int ret;

	// TODO remove MIN()
	// MIN() is here only because underlaying driver (sc16is7xx) currently returns err when
	// passed buffer is too large
	int bytes_read = uart_fifo_read(sat->uart_dev, sat->rx_buf_receive_ptr,
					MIN(RX_MESSAGE_MAX_SIZE - sat->rx_buf_len, 64));

	if (bytes_read < 0) {
		LOG_ERR("Call `uart_fifo_read` failed: %d", bytes_read);
		k_oops();
	}

	if (bytes_read == 0 && RX_MESSAGE_MAX_SIZE - sat->rx_buf_len != 0) {
		LOG_WRN("Call `uart_fifo_read` read 0 bytes!");
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

	LOG_DBG("Received %d bytes from satelite module. contains_stop_byte=%d, "
		"remaining_buffer_size=%d",
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

		LOG_DBG("Disabling UART RX interrupt.");

		uart_irq_rx_disable(sat->uart_dev);
	}
}

static void ctr_sat_handle_tx_irq(struct ctr_sat *sat)
{
	if (sat->tx_buf_len == 0) {
		LOG_DBG("TX completed. No bytes remains to send.");
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

	if (bytes_sent == 0) {
		LOG_ERR("Call `uart_fifo_fill` accepted 0 bytes!");
	}

	sat->tx_buf_transmit_ptr += bytes_sent;
	sat->tx_buf_len -= bytes_sent;

	LOG_DBG("Written %u byte(s) to UART FIFO. %u bytes remains.", bytes_sent, sat->tx_buf_len);
}

static void ctr_sat_uart_callback(const struct device *dev, void *user_data)
{
	// LOG_INF("UART Callback triggered.");

	struct ctr_sat *sat = (struct ctr_sat *)user_data;

	if (!uart_irq_update(sat->uart_dev)) {
		LOG_WRN("uart_irq_update did not allow execution of UART interrupt handler. "
			"Possible spurious interrupt.");
		return;
	}

	int isHandled = 0;

	if (uart_irq_rx_ready(sat->uart_dev)) {
		isHandled = 1;
		LOG_DBG("RX Interrupt is pending.");
		ctr_sat_handle_uart_rx_irq(sat);
	}

	if (uart_irq_tx_ready(sat->uart_dev)) {
		isHandled = 1;
		LOG_DBG("TX interrupt is pending.");
		ctr_sat_handle_tx_irq(sat);
	}

	if (!isHandled) {
		LOG_WRN("Possible spurious UART interrupt. No TX and no RX flag was set.");
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

	sat->last_payload_id = 0;
	sat->enqued_payloads = 0;

	k_mutex_init(&sat->mutex);
	k_mutex_lock(&sat->mutex, K_FOREVER);
	k_poll_signal_init(&sat->rx_completed_signal);

	uart_irq_callback_user_data_set(sat->uart_dev, ctr_sat_uart_callback, sat);

	astronode_cfg_r_answer response;
	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_CFG_R, NULL, 0, (uint8_t *)&response,
				      sizeof(response));
	if (ret < 0) {
		LOG_ERR("Error while getting configuration.");
		return ret;
	}

	k_mutex_unlock(&(sat->mutex));

	return 0;
}

int ctr_sat_set_callback(struct ctr_sat *sat, ctr_sat_message_event_cb user_cb, void *user_data)
{
	sat->callback = user_cb;
	sat->callback_data = user_data;

	return 0;
}

void ctr_sat_get_hwcfg_syscon(struct ctr_sat_hwcfg *hwcfg)
{
	/*
	hwcfg->uart_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_sc16is740_syscon));
	// hwcfg->module_reset_gpio = {.port = NULL, .pin = 2, .dt_flags = GPIO_ACTIVE_LOW};
	hwcfg->module_reset_gpio.port = NULL;
	// GPIO_DT_SPEC_GET(			DT_NODELABEL(ctr_v1_syscon), modem_reset_gpios);
	hwcfg->module_wakeup_gpio =
		GPIO_DT_SPEC_INST_GET(DT_NODELABEL(ctr_v1_syscon), modem_wakeup_gpios);
	hwcfg->module_event_gpio =
		GPIO_DT_SPEC_INST_GET(DT_NODELABEL(ctr_v1_syscon), modem_event_gpios);
	*/
}

void ctr_sat_get_hwcfg_w1(struct ctr_sat_hwcfg *hwcfg)
{
	k_oops();
}

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
int ctr_sat_use_wifi_devkit(struct ctr_sat *sat, const char *ssid, const char *password,
			    const char *api_key)
{
	int ret;

	if (k_is_in_isr()) {
		return -EWOULDBLOCK;
	}

	k_mutex_lock(&sat->mutex, K_FOREVER);

	astronode_wfi_w_request req = {0};

	ret = snprintf(req.ssid, sizeof(req.ssid), "%s", ssid);
	if (ret < 0) {
		LOG_ERR("Call `snprintf` failed: %d", ret);
		k_oops();

	} else if (ret >= sizeof(req.ssid)) {
		LOG_ERR("Wi-Fi SSID length exceeded maximum allowed length.");
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	ret = snprintf(req.password, sizeof(req.password), "%s", password);
	if (ret < 0) {
		LOG_ERR("snprintf failed with return code %d", ret);
		k_oops();
	}
	if (ret >= sizeof(req.password)) {
		LOG_ERR("Wi-Fi password length exceeded maxmimum allowed length.");
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	ret = snprintf(req.api_key, sizeof(req.api_key), "%s", api_key);
	if (ret < 0) {
		LOG_ERR("snprintf failed with return code %d", ret);
		k_mutex_unlock(&sat->mutex);
		return ret;

	} else if (ret != 96) {
		LOG_ERR("API key must be exactly 96 characters. You passed string of length %d.",
			ret);
		k_mutex_unlock(&sat->mutex);
		return -EINVAL;
	}

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_WIF_W, &req, sizeof(req), NULL, 0);
	if (ret) {
		LOG_ERR("Error while executing WIF_W command. Error code: %d", ret);
		k_mutex_unlock(&sat->mutex);
		return ret;
	}

	k_mutex_unlock(&sat->mutex);

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

	k_mutex_lock(&sat->mutex, K_FOREVER);

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
					      (uint8_t *)&ans, sizeof(ans));
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
	k_mutex_unlock(&sat->mutex);

	return ret;
}

int ctr_sat_flush_message(struct ctr_sat *sat, message_handle msg_handle)
{
	return -9999;
}

int ctr_sat_flush_messages(struct ctr_sat *sat)
{
	int ret;

	k_mutex_lock(&sat->mutex, K_FOREVER);

	// ASTRONODE_S_ERROR_BUFFER_EMPTY is allowed error

	ret = ctr_sat_execute_command(sat, ASTRONODE_S_CMD_PLD_F, NULL, 0, NULL, 0);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_execute_command` failed: %d", ret);
		k_mutex_unlock(&sat->mutex);
		return ret;
	} else if (ret) {
		if (ret != ASTRONODE_S_ERROR_BUFFER_EMPTY) {
			LOG_WRN("Satellite module error status: 0x%04x", ret);
			k_mutex_unlock(&sat->mutex);
			return -EIO;
		} else {
			LOG_WRN("Flush was attempted while there were no messages enqueued. Ignore "
				"previous 0x%04x error.",
				ASTRONODE_S_ERROR_BUFFER_EMPTY);
		}
	}

	k_mutex_unlock(&sat->mutex);

	return 0;
}

int ctr_sat_stop(struct ctr_sat *sat)
{
	atomic_set(&(sat->is_started), false);
	return 0;
}
