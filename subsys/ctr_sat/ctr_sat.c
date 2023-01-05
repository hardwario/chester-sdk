
/* CHESTER includes */
#include <chester/ctr_sat.h>
#include <chester/drivers/m8.h>
#include "astronode_s_messages.h"

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/crc.h>

/* Standard includes */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

LOG_MODULE_REGISTER(ctr_sat, CONFIG_CTR_SAT_LOG_LEVEL);

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
	// TODO: Compute more accurate
	// Longest message WIF_W
	#define TX_MESSAGE_MAX_SIZE 820
#else
	// TODO: Compute more accurate.
	// Longest message PLD_E
	#define TX_MESSAGE_MAX_SIZE 400
#endif

#define RX_MESSAGE_MAX_SIZE 1000

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x2_sc16is740_a));

static struct k_poll_signal rx_completed_signal;
static atomic_t is_started = ATOMIC_INIT(0);
static struct k_mutex mutex;

static uint8_t tx_buffer[TX_MESSAGE_MAX_SIZE];
static size_t tx_buffer_len = 0;
static uint8_t* tx_buffer_transmit_ptr;

static uint8_t rx_buffer[RX_MESSAGE_MAX_SIZE];
static size_t rx_buffer_len = 0;
static uint8_t* rx_buffer_receive_ptr;

// TODO comment describing fce
static int ctr_sat_parse_byte(uint8_t* rx_buf_ptr, uint8_t* byte) {
    char byte_parse_buffer[3];
    byte_parse_buffer[0] = rx_buf_ptr[0];
    byte_parse_buffer[1] = rx_buf_ptr[1];
    byte_parse_buffer[2] = '\0';

    int parsed_byte;

    if (sscanf(byte_parse_buffer, "%x", &parsed_byte) != 1) {
		// context-customized error is printed by caller
        return -EINVAL;
    }

    *byte = (uint8_t)parsed_byte;

    return 0;
}

// TODO comment describing fce
static uint8_t* ctr_sat_format_byte(uint8_t* buffer, uint8_t byte) {
	char temp[3];

	int ret = snprintf(temp, sizeof(temp), "%02X", byte);
    if (ret < 0 || ret >= sizeof(temp)) {
	    LOG_ERR("Error while formatting command. snprintf failed.");
		k_oops();
    }

	buffer[0] = temp[0];
	buffer[1] = temp[1];

	return buffer + 2;
}

// TODO comment describing fce
static int ctr_sat_check_message_crc(uint8_t response_id, uint8_t* parameters, size_t parameters_len, uint8_t* rx_buf, size_t rx_buf_len) {
	int ret;
    uint16_t actual_crc = crc16_itu_t(0xFFFF, &response_id, sizeof(response_id)); 
	actual_crc = crc16_itu_t(actual_crc, parameters, parameters_len); 

    uint8_t crc_byte_0;
    uint8_t crc_byte_1;

    ret = ctr_sat_parse_byte(rx_buf + rx_buf_len - 5, &crc_byte_0);
    if (ret) {
        LOG_ERR("Error while parsing response message. Mallformed CRC byte value.");
        return ret;
    }

    ret = ctr_sat_parse_byte(rx_buf + rx_buf_len - 3, &crc_byte_1);
    if (ret) {
        LOG_ERR("Error while parsing response message. Mallformed CRC byte value.");
        return ret;
    }

	uint16_t message_crc = (((uint16_t)crc_byte_1) << 8) | (uint16_t)crc_byte_0;

    if (message_crc != actual_crc) {
        LOG_ERR("CRC mismatch detected when checking CRC of response message. Expected CRC: %04X. Actual CRC: %04X", message_crc, actual_crc);
        // TODO find better return code
        return -EINVAL;
    }

	return 0;
}

// TODO comment describing fce
static int ctr_sat_parse_message_container(uint8_t* response_id, uint8_t* rx_buf, size_t rx_buf_len) {
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

	ret = ctr_sat_parse_byte(rx_buf + 1, response_id);
	if (ret) {
        LOG_ERR("Error while parsing response message. Mallformed reply ID byte received.");
        return -EINVAL;
	}
    
	return 0;
}

// TODO comment describing fce
/*
static int ctr_sat_parse_error_message(uint8_t* rx_buf, size_t rx_buf_len, uint16_t* error_code) {
	uint8_t error_code_byte1;
	uint8_t error_code_byte2;
	uint16_t error_code_val;

	if (rx_buf_len != 12) { *//* 12 == 1 (start byte) + 2 (opcode) + 4 (error code) + 4 (CRC) + 1 (stop byte) *//*
        LOG_ERR("Error while parsing error answer message. Invalid message length. Expected: 12, Received: %d", rx_buf_len);
        return -EINVAL;
	}

	ctr_sat_parse_byte(rx_buf + 3, &error_code_byte1);
	ctr_sat_parse_byte(rx_buf + 5, &error_code_byte2);

	error_code_val = (((uint16_t)error_code_byte1) << 8) | error_code_byte2;
		

	return 0;
}
*/

// TODO comment describing fce
static int ctr_sat_parse_response_data(uint8_t* response_data, size_t response_data_size, uint8_t* rx_buf, size_t rx_buf_len) {
	int ret;

	// check message length
	if (8 + response_data_size * 2 != rx_buf_len) {  /* 8 == 1 (start byte) + 2 (code) + 4 (crc) + 1 (stop byte) */
		LOG_ERR("Received asnwer with invalid length. Expected: %d, Received: %d", 8 + response_data_size * 2, rx_buf_len);
		return -EINVAL;
	}

    for (size_t i = 0; i < response_data_size ; i++) {
        ret = ctr_sat_parse_byte(rx_buf + 3 + i * 2, response_data + i);
        if (ret) {
            LOG_ERR("Error while parsing response message. Mallformed payload byte value.");
            return ret;
        }
    }

	return 0;
}

// TODO comment describing fce
static int ctr_sat_parse_message(uint8_t* response_id, uint8_t* response_data, size_t response_data_size, uint8_t* rx_buf, size_t rx_buf_len, uint16_t* error_code) {
    int ret;

	ret = ctr_sat_parse_message_container(response_id, rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_parse_message_container
		return ret;
	}
	
	if (*response_id == ASTRONODE_S_ANSWER_ERROR) {
		astronode_error_answer error_answer;

		ret = ctr_sat_parse_response_data((uint8_t*)&error_answer, sizeof(error_answer), rx_buf, rx_buf_len);
		if (ret) {
			// error details are printed in ctr_sat_parse_response_data
			return ret;
		}

		ret = ctr_sat_check_message_crc(*response_id, (uint8_t*)&error_answer, sizeof(error_answer), rx_buf, rx_buf_len);
		if (ret) {
			// error details are printed in ctr_sat_check_message_crc
			return ret;
		}

		/*
		ret = ctr_sat_parse_error_message(rx_buf, rx_buf_len, error_code);
		if (ret) {
			// error details are printed in ctr_sat_parse_error_message
			return ret;
		}
		*/

		// if caller passed pointer to error_code variable I assume that it is interested in it 
		// and catch error somehow. In this case I do not LOG_ERR.
		if (error_code) {
			*error_code = error_answer.error_code;
		} else {
			LOG_ERR("Astronode command execution failed with error code 0x%04X.", error_answer.error_code);
		}
		return -EFAULT;
	}

	ret = ctr_sat_parse_response_data(response_data, response_data_size, rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_parse_response_data
		return ret;
	}
	
	ret = ctr_sat_check_message_crc(*response_id, response_data, response_data_size, rx_buf, rx_buf_len);
	if (ret) {
		// error details are printed in ctr_sat_check_message_crc
		return ret;
	}

	return 0;
}

// TODO comment describing fce
static int ctr_sat_execute_command(uint8_t command, uint8_t* parameters, size_t parameters_size, uint8_t* response_data, size_t response_data_size, uint16_t* error_code) {
    int ret;
	
	if (error_code) {
		*error_code = 0;
	}

    size_t encoded_command_size = 
        1 /* start byte */ + 
		2 /* command ID */ +
        2 * parameters_size /* command */ + 
        4 /* CRC */ + 
        1 /* stop byte */;

    size_t encoded_response_size = 
        1 /* start byte */ + 
        2 /* response ID */ + 
        2 * response_data_size /* command */ + 
        4 /* CRC */ + 
        1 /* stop byte */;

    if (encoded_command_size > TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded command is too large.");
        return -EINVAL;
    }
    
    if (encoded_response_size > TX_MESSAGE_MAX_SIZE) {
		LOG_ERR("Encoded response is too large.");
        return -EINVAL;
    }

	uint8_t* tx_buf_wrptr = tx_buffer;

    *tx_buf_wrptr++ = ASTRONODE_S_START_BYTE;

	tx_buf_wrptr = ctr_sat_format_byte(tx_buf_wrptr, command);

    for (size_t i = 0; i < parameters_size; i++) {
		tx_buf_wrptr = ctr_sat_format_byte(tx_buf_wrptr, parameters[i]);
    }

    uint16_t crc = crc16_itu_t(0xFFFF, &command, sizeof(command));
    crc = crc16_itu_t(crc, parameters, parameters_size);

	tx_buf_wrptr = ctr_sat_format_byte(tx_buf_wrptr, (uint8_t)((crc & 0x00FF) >> 0));
	tx_buf_wrptr = ctr_sat_format_byte(tx_buf_wrptr, (uint8_t)((crc & 0xFF00) >> 8));
    
    *tx_buf_wrptr++ = ASTRONODE_S_END_BYTE;

    if (tx_buf_wrptr > tx_buffer + sizeof(tx_buffer)) {
	    LOG_ERR("Error while formatting command. Stack possibly corrupted.");
        k_oops();
    }
    
	tx_buffer_transmit_ptr = tx_buffer;
	tx_buffer_len = encoded_command_size;

	rx_buffer_receive_ptr = rx_buffer;
	rx_buffer_len = 0;

	LOG_DBG("Starting UART transmission. To send: %d bytes. To receive: %d bytes.", encoded_command_size, encoded_response_size);
	LOG_HEXDUMP_INF(tx_buffer, encoded_command_size, "Encoded command to astronode:");

	k_poll_signal_reset(&rx_completed_signal);

	uart_irq_rx_enable(dev);
    uart_irq_tx_enable(dev);

	// wait for reply
	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &rx_completed_signal),
	};
	
    // TODO: Check that timeout matches astronode specification. Take in account
	// delays of transsmision and recpetion delays due to complicated chip 
	// structure (I2C, 1W and UART bridges)
	// Following timeout must also take into account delay of command transmission. Astronode UART is only 9600 bauds.
	ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(100 + 1000 * encoded_command_size * 10 / 9600));
	if (ret == -EAGAIN) {
		LOG_ERR("Command failed because Astronode module did not respond within specified time. This may happen when module is disconnected.");
		return ret;
	}
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed when waiting for completion of transmitting astronode command and receiving reply. Code: %d", ret);
		return ret;
	}

	LOG_DBG("UART transmission completed. Sent: %d bytes. Received: %d bytes.", encoded_command_size, rx_buffer_len);
	LOG_HEXDUMP_INF(rx_buffer, rx_buffer_len, "Encoded answer from astronode:");

	if (rx_buffer_len != encoded_response_size) {
		LOG_WRN("Received unexpected answer with unexpected length. Expected: %d, Received: %d", encoded_response_size, rx_buffer_len);
	}

	uint8_t response_id;
    ret = ctr_sat_parse_message(&response_id, response_data, response_data_size, rx_buffer, rx_buffer_len, error_code);
	if (ret) {
		// error details printed in ctr_sat_parse_message
		return ret;
	}

	uint8_t expected_response_code = ASTRONODE_S_ANSWER_CODE(command);
	if (response_id != expected_response_code) {
		LOG_ERR("Astronode replied with unexpected response. Request code: %02X, expected response code: %02X, received response code: %02X", command, expected_response_code, response_id);
        return -EINVAL;
	}

	return 0;
}

static void ctr_sat_handle_uart_rx_irq(const struct device *dev) {
	int ret;

	// TODO remove MIN()
	// MIN() is here only because underlaying driver (sc16is7xx) currently returns err when passed buffer is too large
	int bytes_read = uart_fifo_read(dev, rx_buffer_receive_ptr, MIN(RX_MESSAGE_MAX_SIZE - rx_buffer_len, 64));
		
	if (bytes_read < 0) {
		LOG_ERR("Call `uart_fifo_read` failed: %d", bytes_read);
		k_oops();
	}

	if (bytes_read == 0 && RX_MESSAGE_MAX_SIZE - rx_buffer_len != 0) {
		LOG_WRN("Call `uart_fifo_read` read 0 bytes!");
	}

	int contains_stop_byte = 0;

	for (int i = 0; i < bytes_read; i++) {
		if (rx_buffer_receive_ptr[i] == ASTRONODE_S_END_BYTE) {
			contains_stop_byte = 1;
			// stop byte should be last received byte
			if (i != bytes_read - 1) {
				LOG_WRN("Stop byte was received but satelite module send additional data after stop byte!");
			}
			break;
		}
	}

	rx_buffer_receive_ptr += bytes_read;
	rx_buffer_len += bytes_read;

	LOG_DBG("Received %d bytes from satelite module. contains_stop_byte=%d, remaining_buffer_size=%d", bytes_read, contains_stop_byte, RX_MESSAGE_MAX_SIZE - rx_buffer_len);

	if (RX_MESSAGE_MAX_SIZE - rx_buffer_len == 0 || contains_stop_byte) {
		int status;

		if (contains_stop_byte) {
			status = 0;
		} else {
			status = -ENOSPC;
		}

		ret = k_poll_signal_raise(&rx_completed_signal, status);
		if (ret < 0) {
			LOG_ERR("Call `k_poll_signal_raise` failed with status %d!", ret);
			k_oops();
		}

		LOG_DBG("Disabling UART RX interrupt.");

		uart_irq_rx_disable(dev);
	}

}

static void ctr_sat_handle_tx_irq(const struct device *dev) {
	if (tx_buffer_len == 0) {
		LOG_DBG("TX completed. No bytes remains to send.");
		uart_irq_tx_disable(dev);
		return;
	}

	// TODO remove MIN()
	// MIN() is here only because underlaying driver (sc16is7xx) currently returns err when passed buffer is too large
	int bytes_sent = uart_fifo_fill(dev, tx_buffer_transmit_ptr, MIN(tx_buffer_len, 64));
	if (bytes_sent < 0) {
		LOG_ERR("Call `uart_fifo_fill` failed: %d", bytes_sent);
		k_oops();
	}

	if (bytes_sent == 0) {
		LOG_ERR("Call `uart_fifo_fill` accepted 0 bytes!");
		k_oops();
	}

	tx_buffer_transmit_ptr += bytes_sent;
	tx_buffer_len -= bytes_sent;

	LOG_DBG("Written %u byte(s) to UART FIFO. %u bytes remains.", bytes_sent, tx_buffer_len);
}

static void ctr_sat_uart_callback(const struct device *dev, void *user_data) {
	LOG_INF("UART Callback triggered.");

	if (!uart_irq_update(dev)) {
		LOG_WRN("uart_irq_update did not allow execution of UART interrupt handler. Possible spurious interrupt.");
		return;
	}

	int isHandled = 0;

	if (uart_irq_rx_ready(dev)) {
		isHandled = 1;
		LOG_DBG("RX Interrupt is pending.");
		ctr_sat_handle_uart_rx_irq(dev);
	}

	if (uart_irq_tx_ready(dev)) {
		isHandled = 1;
		LOG_DBG("TX interrupt is pending.");
		ctr_sat_handle_tx_irq(dev);
	}

	if (!isHandled) {
		LOG_WRN("Possible spurious UART interrupt. No TX and no RX flag was set.");
	}
}

int ctr_sat_start() {
	int ret;

	if (!device_is_ready(dev)) {
		LOG_ERR("Device not ready");
		k_oops();
	}
	
	uint8_t deadline = 10;
	if (k_mutex_lock(&mutex, K_SECONDS(deadline)) != 0) {
		LOG_ERR("Unable to acquire mutex for more than %d seconds!.", deadline);
		return -EDEADLOCK;
	}

	k_poll_signal_init(&rx_completed_signal);

	uart_irq_callback_user_data_set(dev, ctr_sat_uart_callback, NULL);

	astronode_cfg_r_answer response;
	ret = ctr_sat_execute_command(ASTRONODE_S_CMD_CFG_R, NULL, 0, (uint8_t*)&response, sizeof(response), NULL);
	if (ret < 0) {
		LOG_ERR("Error while getting configuration.");
		return ret;
	}

	atomic_set(&is_started, 1);
	
	if (k_mutex_unlock(&mutex) != 0) {
		LOG_ERR("Unable to release mutex!");
		k_oops();
	}
	
	return 0;
}

#if CONFIG_CTR_SAT_USE_WIFI_DEVKIT
int ctr_sat_setup_wifi_devkit(char* ssid, char* password, char* api_key) {
	int ret;
	astronode_wfi_w_request req;

	uint8_t deadline = 10;
	if (k_mutex_lock(&mutex, K_SECONDS(deadline)) != 0) {
		LOG_ERR("Unable to acquire mutex for more than %d seconds!.", deadline);
		return -EDEADLOCK;
	}

	if (atomic_get(&is_started) == 0) {
		LOG_ERR("satelite module is not started. Please call ctr_sat_start() first.");
		return -EACCES;
	}

	memset(&req, 0, sizeof(req));
	
	ret = snprintf(req.ssid, sizeof(req.ssid), "%s", ssid);
	if (ret < 0) {
		LOG_ERR("snprintf failed with return code %d", ret);
		k_oops();
	}
	if (ret >= sizeof(req.ssid)) {
		LOG_ERR("Wi-Fi SSID length exceeded maxmimum allowed length.");
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
		k_oops();
	}
	if (ret != 96) {
		LOG_ERR("API key must be exactly 96 characters. You passed string of length %d.", ret);
		return -EINVAL;
	}

	ret = ctr_sat_execute_command(ASTRONODE_S_CMD_WIF_W, (uint8_t*)&req, sizeof(req), NULL, 0, NULL);
	if (ret) {
		LOG_ERR("Error while executing WIF_W command. Error code: %d", ret);
		return ret;
	}

	if (k_mutex_unlock(&mutex) != 0) {
		LOG_ERR("Unable to release mutex!");
		k_oops();
	}

	return 0;
}
#endif

int ctr_sat_send_message(uint8_t* message, size_t message_size, ctr_sat_message_event_callback event_callback, void* event_callback_user_data) {
	int ret;
	astronode_pld_e_request req;
	astronode_pld_e_answer ans;
	
	uint8_t deadline = 10;
	if (k_mutex_lock(&mutex, K_SECONDS(deadline)) != 0) {
		LOG_ERR("Unable to acquire mutex for more than %d seconds!.", deadline);
		return -EDEADLOCK;
	}

	if (atomic_get(&is_started) == 0) {
		LOG_ERR("satelite module is not started. Please call ctr_sat_start() first.");
		ret = -EACCES;
		goto unlock_and_exit;
	}

	if (message_size > 160) {
		LOG_DBG("Message is too long.");
		ret = -EINVAL;
		goto unlock_and_exit;
	}

	req.id = 1;
	memcpy(req.message, message, message_size);

	ret = ctr_sat_execute_command(ASTRONODE_S_CMD_PLD_E, (uint8_t*)&req, 2 + message_size, (uint8_t*)&ans, sizeof(ans), NULL);
	if (ret) {
		LOG_ERR("Error while executing PLD_E command. Error code: %d", ret);
		goto unlock_and_exit;
	}
	
unlock_and_exit:	
	if (k_mutex_unlock(&mutex) != 0) {
		LOG_ERR("Unable to release mutex!");
		k_oops();
	}

	return ret;
}

int ctr_sat_revoke_all_messages() {
	int ret;
	uint16_t error_code;

	// ASTRONODE_S_ERROR_BUFFER_EMPTY is allowed error

	ret = ctr_sat_execute_command(ASTRONODE_S_CMD_PLD_F, NULL, 0, NULL, 0, &error_code);
	if (ret && (ret != -EFAULT || error_code != ASTRONODE_S_ERROR_BUFFER_EMPTY)) {
		LOG_ERR("ctr_sat_execute_command failed with status %d", ret);
		return ret;
	}
	

	return 0;
}

int ctr_sat_stop() {
	
	atomic_set(&is_started, 0);

	return -999;
}
