#include "app_config.h"
#include "app_data.h"
#include "app_handler.h"
#include "app_shell.h"
#include "app_work.h"
#include "packet.h"
#include "wmbus.h"

/* CHESTER includes */
#include <chester/ctr_gpio.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>

/* Standard includes */
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(wmbus, LOG_LEVEL_DBG);

#define MBUS_OFFSET_MANUFACTURER           2
#define MBUS_OFFSET_ADDRESS                4
#define MBUS_OFFSET_ADDRESS_DEVICE_VERSION 8
#define MBUS_OFFSET_ADDRESS_DEVICE_TYPE    9

bool flags[DEVICE_MAX_COUNT];

static const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(uart1));

struct onoff_client m_onoff_cli;
struct onoff_manager *m_onoff_mgr;

struct k_poll_signal tx_done_sig;

RING_BUF_DECLARE(m_tx_ring_buf, 64);

static int address_to_index(uint32_t address)
{
	for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
		if (g_app_config.address[i] == address) {
			return i;
		}
	}

	return -ENOENT;
}

void wmbus_parse_packet(uint8_t *packet, size_t len, wmbus_packet_meta *meta)
{
	meta->manufacturer_id = sys_get_le16(&packet[MBUS_OFFSET_MANUFACTURER]);
	meta->address_bcd = sys_get_le32(&packet[MBUS_OFFSET_ADDRESS]);
	meta->device_type = packet[MBUS_OFFSET_ADDRESS_DEVICE_TYPE];
	meta->device_version = packet[MBUS_OFFSET_ADDRESS_DEVICE_VERSION];
	meta->address = wmbus_convert_bcd_to_uint32(meta->address_bcd);
	meta->rssi_dbm = wmbus_convert_rssi_to_dbm(packet[len - 1]);

	meta->manufacturer_name[0] = ((meta->manufacturer_id >> 10) & 0x1F) + 64;
	meta->manufacturer_name[1] = ((meta->manufacturer_id >> 5) & 0x1F) + 64;
	meta->manufacturer_name[2] = (meta->manufacturer_id & 0x1F) + 64;
	meta->manufacturer_name[3] = '\0'; // null terminator
}

bool wmbus_set_and_check_address_flag(uint32_t address)
{
	int index = address_to_index(address);

	if (index < 0) {
		return 0;
	}

	/* Return true only once */
	bool retval = flags[index] == false;
	flags[index] = true;

	return retval;
}

void wmbus_clear_address_flags(void)
{
	for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
		flags[i] = false;
	}
}

void wmbus_get_config_device_count(size_t *count)
{
	*count = 0;

	for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
		if (g_app_config.address[i] != 0) {
			(*count)++;
		}
	}
}

bool wmbus_check_all_received_flags(void)
{
	bool no_devices_configured = true;

	for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
		if (g_app_config.address[i] != 0) {
			no_devices_configured = false;
			if (flags[i] == false) {
				return false;
			}
		}
	}

	/* If no devices configured, never return true */
	if (no_devices_configured) {
		return false;
	}

	return true;
}

/* Zephyr doesn't have uint32 variant */
uint32_t wmbus_convert_bcd_to_uint32(uint32_t address)
{
	uint64_t mask = 0x000f;
	uint64_t pwr = 1;

	uint64_t i = (address & mask);
	while ((address = (address >> 4))) {
		pwr *= 10;
		i += (address & mask) * pwr;
	}
	return (uint32_t)i;
}

int wmbus_convert_rssi_to_dbm(uint8_t rssi_raw)
{
	if (rssi_raw >= 128) {
		return (rssi_raw - 256) / 2 - 74;
	} else {
		return rssi_raw / 2 - 74;
	}
}

#define RX_BUF_SIZE 512
uint8_t rx_buf[RX_BUF_SIZE];
uint8_t rx_buf_length = 0;
uint8_t checksum;
uint8_t packet_len;

static void uart_rx_timeout(struct k_timer *timer_id)
{
	rx_buf_length = 0;
}

static K_TIMER_DEFINE(m_uart_rx_timeout, uart_rx_timeout, NULL);

size_t wmbus_rx_byte(uint8_t b)
{
	k_timer_start(&m_uart_rx_timeout, K_MSEC(50), K_FOREVER);

	if (rx_buf_length == 0) {
		if (b != 0xff) {
			rx_buf_length = 0;
			LOG_INF("%02x != 0xff, ", b);
			return 0;
		}
		checksum = 0;
		checksum ^= b;
		rx_buf[rx_buf_length++] = b;
	} else if (rx_buf_length == 1) {
		/* CMD_DATA_IND */
		if (b != 0x03) {
			rx_buf_length = 0;
			LOG_INF("b != 0x03");
			return 0;
		}
		checksum ^= b;
		rx_buf[rx_buf_length++] = b;
	} else if (rx_buf_length == 2) {
		packet_len = b;
		checksum ^= b;
		rx_buf[rx_buf_length++] = b;

	} else if (rx_buf_length >= 3 && (rx_buf_length - 4) < packet_len) {
		rx_buf[rx_buf_length++] = b;

		if ((rx_buf_length - 4) == packet_len) {
			size_t len_ret = rx_buf_length;
			rx_buf_length = 0;

			return (b == checksum) ? len_ret : 0;
		}

		/* XOR all bytes except the last one with CS */
		checksum ^= b;
	}

	return 0;
}

uint8_t *wmbus_get_buffer(void)
{
	return rx_buf;
}

static int hfclk_request(void)
{
	int ret;

	static struct k_poll_signal sig;

	k_poll_signal_init(&sig);
	sys_notify_init_signal(&m_onoff_cli.notify, &sig);

	ret = onoff_request(m_onoff_mgr, &m_onoff_cli);
	if (ret < 0) {
		LOG_ERR("Call `onoff_request` failed: %d", ret);
		return ret;
	}

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);
	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int hfclk_release(void)
{
	int ret;

	ret = onoff_cancel_or_release(m_onoff_mgr, &m_onoff_cli);
	if (ret < 0) {
		LOG_ERR("Call `onoff_cancel_or_release` failed: %d", ret);
		return ret;
	}

	return 0;
}

int wmbus_enable(void)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	enum pm_device_state device_state;
	ret = pm_device_state_get(uart_dev, &device_state);
	if (ret) {
		LOG_ERR("Call `pm_device_state_get` failed: %d", ret);
		return ret;
	}

	if (device_state == PM_DEVICE_STATE_SUSPENDED) {
		ret = pm_device_action_run(uart_dev, PM_DEVICE_ACTION_RESUME);
		if (ret) {
			LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
			return ret;
		}
	}

	hfclk_request();

	uart_irq_rx_enable(uart_dev);

	/* Power-on Wurth module */
	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_A0, 1);
	if (ret) {
		LOG_ERR("Failed ctr_gpio_write channel %d", ret);
		return ret;
	}

	return 0;
}

int wmbus_disable(void)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	enum pm_device_state device_state;
	ret = pm_device_state_get(uart_dev, &device_state);
	if (ret) {
		LOG_ERR("Call `pm_device_state_get` failed: %d", ret);
		return ret;
	}

	if (device_state == PM_DEVICE_STATE_ACTIVE) {
		ret = pm_device_action_run(uart_dev, PM_DEVICE_ACTION_SUSPEND);
		if (ret) {
			LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
			return ret;
		}
	}

	uart_irq_rx_disable(uart_dev);

	hfclk_release();

	/* Power-off Wurth module */
	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_A0, 0);
	if (ret) {
		LOG_ERR("Failed ctr_gpio_write channel %d", ret);
		return ret;
	}

	return 0;
}

int wmbus_antenna_set(int ant)
{
	int ret;

	/* ANT1 */
	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_A2, ant == 1 ? 1 : 0);
	if (ret) {
		LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
	}

	/* ANT2 */
	ret = ctr_gpio_write(CTR_GPIO_CHANNEL_A3, ant == 2 ? 1 : 0);
	if (ret) {
		LOG_ERR("Call `ctr_gpio_write` failed: %d", ret);
	}

	return 0;
}

static int scan_all_check_and_add(uint32_t address)
{
	/* Check if address already exists */
	for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
		if (g_app_config.address[i] == address) {
			LOG_WRN("Address %d already exists", address);
			return -EEXIST;
		}
	}

	/* Find next empty */
	int next_empty = -1;
	for (int i = 0; i < DEVICE_MAX_COUNT; i++) {
		if (g_app_config.address[i] == 0) {
			next_empty = i;
			break;
		}
	}

	if (next_empty == -1) {
		LOG_WRN("No empty position");
		return -ENOSPC;
	}

	g_app_config.address[next_empty] = address;

	return 0;
}

static int mbus_packet_handle(uint8_t *data, size_t len)
{
	int ret;
	/* Skip frame header byte and frame type byte from Wurth Module */
	uint8_t *mbus = &data[2];
	/* Keep RSSI in packet/len */
	size_t mbus_len = len - 3;

	wmbus_packet_meta meta;

	wmbus_parse_packet(mbus, mbus_len, &meta);

	// TODO for debug log only our address
	if (meta.address == 81763000) {
		LOG_WRN("%d, Mnf: %s, RSSI: %d, len: %d", meta.address, meta.manufacturer_name,
			meta.rssi_dbm, mbus_len);
	} else {
		LOG_INF("%d, Mnf: %s, RSSI: %d, len: %d", meta.address, meta.manufacturer_name,
			meta.rssi_dbm, mbus_len);
	}

	// LOG_HEXDUMP_INF(mbus, mbus_len, "wM-BUS packet push");

	char enrolled_str[32] = "";

	// If no devices configured, add all received addresses to config before next step
	if (g_app_data.scan_all) {
		if (!g_app_data.enroll_mode) {
			scan_all_check_and_add(meta.address);
		} else {
			// If in teach-mode, skip weak signals
			if (meta.rssi_dbm >= g_app_data.enroll_rssi_threshold) {
				scan_all_check_and_add(meta.address);
				LOG_INF("Added in teach-mode %d dBm (threshold %d dBm)",
					meta.rssi_dbm, g_app_data.enroll_rssi_threshold);
				strncpy(enrolled_str, "saved", sizeof(enrolled_str));
			} else {
				LOG_INF("Weak RSSI %d dBm (threshold %d dBm)", meta.rssi_dbm,
					g_app_data.enroll_rssi_threshold);
				strncpy(enrolled_str, "weak", sizeof(enrolled_str));
			}
		}
	}

	struct shell *shell = app_shell_get();
	if (shell) {
		shell_print(shell, "%d, Mnf: %s, RSSI: %d, %s", meta.address,
			    meta.manufacturer_name, meta.rssi_dbm, enrolled_str);
	}

	/* Check if address matches */
	if (wmbus_set_and_check_address_flag(meta.address)) {
		ret = packet_push(mbus, mbus_len);
		if (ret) {
			LOG_ERR("Call `packet_push` failed: %d", ret);
			return ret;
		}
	}

	/* Check only when addresses are configured */
	if (!g_app_data.scan_all && wmbus_check_all_received_flags()) {
		/* All packets already received */
		app_work_send_trigger();
		g_app_data.scan_stop_timestamp = k_uptime_get();
	}

	return 0;
}

void app_handler_uart_callback(const struct device *dev, void *app_data)
{
	int ret;

	if (!uart_irq_update(dev)) {
		return;
	}

	if (uart_irq_rx_ready(uart_dev)) {
		for (;;) {
			static uint8_t buf[64];
			int bytes_read = uart_fifo_read(uart_dev, buf, sizeof(buf));
			if (bytes_read < 0) {
				LOG_ERR("Call `uart_fifo_read` failed: %d", bytes_read);
				k_oops();
			}

			if (!bytes_read) {
				break;
			}

			for (int i = 0; i < bytes_read; i++) {
				size_t len;
				if ((len = wmbus_rx_byte(buf[i]))) {
					ret = mbus_packet_handle(wmbus_get_buffer(), len);
					if (ret) {
						LOG_ERR("Call `mbus_packet_handle` failed: %d",
							ret);
					}
				}
			}
		}
	}

	if (uart_irq_tx_ready(dev)) {
		LOG_DBG("TX interrupt");

		for (;;) {
			uint8_t *buf;
			uint32_t claimed = ring_buf_get_claim(&m_tx_ring_buf, &buf, 16);
			if (!claimed) {
				uart_irq_tx_disable(dev);
				k_poll_signal_raise(&tx_done_sig, 0);
				break;
			}

			LOG_DBG("Claimed %u byte(s) from TX ring buffer", claimed);

			int bytes_sent = uart_fifo_fill(dev, buf, claimed);
			if (bytes_sent < 0) {
				LOG_ERR("Call `uart_fifo_fill` failed: %d", bytes_sent);
				k_oops();
			}

			ret = ring_buf_get_finish(&m_tx_ring_buf, bytes_sent);
			if (ret) {
				LOG_ERR("Call `ring_buf_get_finish` failed: %d", ret);
				k_oops();
			}

			LOG_DBG("Consumed %u byte(s) from TX ring buffer", bytes_sent);

			if (bytes_sent != claimed) {
				// error?
				k_poll_signal_raise(&tx_done_sig, 0);
				break;
			}
		}
	}
}

int wmbus_init(void)
{

	int ret;

	LOG_INF("System initialization");

	k_poll_signal_init(&tx_done_sig);

	/* Init onoff manager handler */
	m_onoff_mgr = z_nrf_clock_control_get_onoff(CLOCK_CONTROL_NRF_SUBSYS_HF);
	if (m_onoff_mgr == NULL) {
		LOG_ERR("Call `z_nrf_clock_control_get_onoff` failed");
		return -ENXIO;
	}

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	struct uart_config uart_cfg = {0};

	uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
	uart_cfg.baudrate = 9600;
	uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
	uart_cfg.parity = UART_CFG_PARITY_NONE;

	ret = uart_configure(uart_dev, &uart_cfg);
	if (ret) {
		LOG_ERR("Call `uart_configure` failed: %d", ret);
		return ret;
	}

	ret = pm_device_action_run(uart_dev, PM_DEVICE_ACTION_SUSPEND);
	if (ret) {
		LOG_ERR("Call `pm_device_action_run` failed: %d", ret);
		return ret;
	}

	uart_irq_callback_user_data_set(uart_dev, app_handler_uart_callback, NULL);

	/* Init ANT1 switch */
	ret = ctr_gpio_set_mode(CTR_GPIO_CHANNEL_A2, CTR_GPIO_MODE_OUTPUT);
	if (ret) {
		LOG_ERR("Failed initializing channel A2: %d", ret);
		return ret;
	}

	/* Init ANT2 switch */
	ret = ctr_gpio_set_mode(CTR_GPIO_CHANNEL_A3, CTR_GPIO_MODE_OUTPUT);
	if (ret) {
		LOG_ERR("Failed initializing channel A3: %d", ret);
		return ret;
	}

	/* Init WM_ON output */
	ret = ctr_gpio_set_mode(CTR_GPIO_CHANNEL_A0, CTR_GPIO_MODE_OUTPUT);
	if (ret) {
		LOG_ERR("Failed initializing channel A0: %d", ret);
		return ret;
	}

	/* When no devices configured, mark scan_all flag */
	size_t device_count;
	wmbus_get_config_device_count(&device_count);
	g_app_data.scan_all = device_count == 0;

	LOG_INF("Initialization end");

	return 0;
}

int send_single_command(uint8_t *buf, size_t size)
{
	int ret;

	if (!device_is_ready(uart_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	uint32_t written = ring_buf_put(&m_tx_ring_buf, buf, size);
	LOG_DBG("Wrote %u byte(s) to TX ring buffer", written);

	if (written != size) {
		LOG_ERR("Not enough space in TX ring buffer");
	}

	k_poll_signal_reset(&tx_done_sig);

	uart_irq_tx_enable(uart_dev);

	struct k_poll_event events[] = {
		K_POLL_EVENT_INITIALIZER(K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &tx_done_sig),
	};

	ret = k_poll(events, ARRAY_SIZE(events), K_FOREVER);

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	/* We need to wait for Wurth to handle message */
	k_sleep(K_MSEC(200));

	return 0;
}

int wmbus_configure(const struct shell *shell, size_t argc, char **argv)
{

	for (int i = 0; i < argc; i++) {
		shell_print(shell, "%d: %s", i, argv[i]);
	}

	if (argc == 1) {

		const uint8_t wurth_cmd_disable_sleep[] = {0xFF, 0x09, 0x03, 0x3F,
							   0x01, 0x00, 0xCB};
		send_single_command((uint8_t *)wurth_cmd_disable_sleep,
				    sizeof(wurth_cmd_disable_sleep));

		const uint8_t wurth_cmd_append_rssi[] = {0xFF, 0x09, 0x03, 0x45, 0x01, 0x01, 0xB0};
		send_single_command((uint8_t *)wurth_cmd_append_rssi,
				    sizeof(wurth_cmd_append_rssi));

		const uint8_t wurth_cmd_enable_command_mode[] = {0xFF, 0x09, 0x03, 0x05,
								 0x01, 0x01, 0xF0};
		send_single_command((uint8_t *)wurth_cmd_enable_command_mode,
				    sizeof(wurth_cmd_enable_command_mode));

		const uint8_t wurth_cmd_set_t2_c2_other[] = {0xFF, 0x09, 0x03, 0x46,
							     0x01, 0x09, 0xBB};
		send_single_command((uint8_t *)wurth_cmd_set_t2_c2_other,
				    sizeof(wurth_cmd_set_t2_c2_other));

		return 0;

	} else if (argc == 2) {

		if (strcmp(argv[1], "reset") == 0) {

			const uint8_t wurth_cmd_factory_reset[] = {0xFF, 0x11, 0x00, 0xEE};
			send_single_command((uint8_t *)wurth_cmd_factory_reset,
					    sizeof(wurth_cmd_factory_reset));

			return 0;

		} else {
			uint8_t buf[32];
			size_t dst_size = strlen(argv[1]) / 2;

			if (dst_size > sizeof(buf)) {
				LOG_ERR("Buffer too small: %d", dst_size);
				return -ENOMEM;
			}

			int ret = ctr_hex2buf(argv[1], buf, dst_size, true);

			if (ret < 0) {
				LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
				return ret;
			}

			send_single_command(buf, ret);

			return 0;
		}
	}

	return 0;
}
