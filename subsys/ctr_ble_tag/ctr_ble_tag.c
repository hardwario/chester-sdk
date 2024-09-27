/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_ble_tag.h>
#include <chester/ctr_config.h>
#include <chester/ctr_info.h>
#include <chester/ctr_util.h>

/* Zephyr includes */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_ble_tag, CONFIG_CTR_BLE_TAG_LOG_LEVEL);

#define SETTINGS_PFX "ble_tag"

#define SCAN_PARAMS_DEFAULTS                                                                       \
	{                                                                                          \
		.type = BT_LE_SCAN_TYPE_ACTIVE,                                                    \
		.options = BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST,                                      \
		.interval = CTR_BLE_TAG_SCAN_MAX_TIME,                                             \
		.window = CTR_BLE_TAG_SCAN_MAX_TIME,                                               \
	}

struct config {
	bool enabled;
	int scan_interval;
	int scan_duration;
	uint8_t addr[CTR_BLE_TAG_COUNT][BT_ADDR_SIZE];
};

static struct config m_config_interim = {
	.scan_interval = 300,
	.scan_duration = 12,
};

static struct config m_config;

struct ble_tag_data {
	bool valid;
	int64_t timestamp;
	int rssi;
	float temperature;
	float humidity;
	float voltage;
};

static struct ble_tag_data m_tag_data[CTR_BLE_TAG_COUNT];
static K_MUTEX_DEFINE(m_tag_data_lock);

static char m_enroll_addr_str[BT_ADDR_SIZE * 2 + 1];

static K_SEM_DEFINE(m_has_enrolled_sem, 0, 1);

static K_MUTEX_DEFINE(m_scan_lock);

static struct k_work_q m_scan_work_q;
static K_THREAD_STACK_DEFINE(m_scan_work_q_stack, 2048);

static int parse_data(struct net_buf_simple *buf, float *temperature, float *humidity,
		      float *voltage)
{
	bool has_data = false;

	if (buf->len < 1) {
		LOG_DBG("Invalid length: %d", buf->len);
		return -EINVAL;
	}

	do {
		uint8_t len = net_buf_simple_pull_u8(buf);
		if (len > buf->len) {
			LOG_DBG("Invalid length: %d, needed: %d", buf->len, len);
			return -EINVAL;
		}

		uint8_t type = net_buf_simple_pull_u8(buf);
		if (type != 0xff) {
			net_buf_simple_pull_mem(buf, len - 1);
			continue;
		}

		uint16_t company_id = net_buf_simple_pull_le16(buf);
		if (company_id != 0x089a) {
			LOG_DBG("Invalid company ID: %x", company_id);
			return -EINVAL;
		}

		uint8_t version = net_buf_simple_pull_u8(buf);
		if (version != 0x01) {
			LOG_DBG("Invalid version: %d", version);
			return -EINVAL;
		}

		uint8_t data_flags = net_buf_simple_pull_u8(buf);

		int16_t temperature_;
		uint8_t humidity_;
		uint8_t voltage_;

		if (data_flags & BIT(0)) {
			if (buf->len < 2) {
				LOG_DBG("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint16_t));
				return -EINVAL;
			}

			temperature_ = net_buf_simple_pull_be16(buf);

			if (temperature) {
				*temperature = temperature_ / 100.0;
				LOG_DBG("Temperature: %.2f C", *temperature);
			}
		}

		if (data_flags & BIT(1)) {
			if (buf->len < 1) {
				LOG_DBG("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint8_t));
				return -EINVAL;
			}

			humidity_ = net_buf_simple_pull_u8(buf);

			if (humidity) {
				*humidity = humidity_;
				LOG_DBG("Humidity: %.0f %%", *humidity);
			}
		}

		if (data_flags & BIT(7)) {
			if (buf->len < 1) {
				LOG_ERR("Invalid length: %d, needed: %d", buf->len,
					sizeof(uint8_t));
				return -EINVAL;
			}

			net_buf_simple_pull_mem(buf, buf->len - 1);

			voltage_ = net_buf_simple_pull_u8(buf);

			if (voltage) {
				*voltage = (voltage_ * 10) + 2000.0;
				LOG_DBG("Voltage: %.2f V", *voltage);
			}
		}

		has_data = true;

	} while (buf->len);

	return has_data ? 0 : -EINVAL;
}

static void update_tag_data(const bt_addr_le_t *addr, bool interim, int8_t rssi, float temperature,
			    float humidity, float voltage)
{
	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	size_t slot = 0;

	for (; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (!memcmp(addr->a.val,
			    interim ? m_config_interim.addr[slot] : m_config.addr[slot],
			    BT_ADDR_SIZE)) {
			break;
		}
	}

	if (slot == CTR_BLE_TAG_COUNT) {
		k_mutex_unlock(&m_tag_data_lock);
		return;
	}

	m_tag_data[slot].timestamp = k_uptime_get();
	m_tag_data[slot].rssi = rssi;
	m_tag_data[slot].temperature = temperature;
	m_tag_data[slot].humidity = humidity;
	m_tag_data[slot].voltage = voltage;

	m_tag_data[slot].valid = true;

	k_mutex_unlock(&m_tag_data_lock);
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	float temperature;
	float humidity;
	float voltage;

	if (parse_data(buf, &temperature, &humidity, &voltage)) {
		return;
	}

	update_tag_data(addr, false, rssi, temperature, humidity, voltage);
}

static void enroll_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		      struct net_buf_simple *buf)
{
	int ret;

	float temperature;
	float humidity;
	float voltage;

	if (parse_data(buf, &temperature, &humidity, &voltage)) {
		return;
	}

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (!memcmp(addr->a.val, m_config_interim.addr[slot], BT_ADDR_SIZE)) {
			return;
		}
	}

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (rssi < CTR_BLE_TAG_ENROLL_RSSI_THRESHOLD) {
			break;
		}

		if (!ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
			continue;
		}

		memcpy(m_config_interim.addr[slot], (void *)addr->a.val, BT_ADDR_SIZE);

		uint8_t swap_addr[BT_ADDR_SIZE];
		sys_memcpy_swap(swap_addr, m_config_interim.addr[slot], BT_ADDR_SIZE);

		ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, m_enroll_addr_str,
				  sizeof(m_enroll_addr_str), false);
		if (ret < 0) {
			LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
			return;
		}

		LOG_INF("Enrolled: %s", m_enroll_addr_str);

		k_sem_give(&m_has_enrolled_sem);

		break;
	}

	update_tag_data(addr, true, rssi, temperature, humidity, voltage);
}

static void start_scan_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(m_start_scan_work, start_scan_work_handler);

static void stop_scan_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(m_stop_scan_work, stop_scan_work_handler);

static void start_scan_work_handler(struct k_work *work)
{
	int ret;

	LOG_DBG("Starting scan...");

	k_mutex_lock(&m_scan_lock, K_FOREVER);

	struct bt_le_scan_param param = SCAN_PARAMS_DEFAULTS;

	ret = bt_le_scan_start(&param, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed: %d", ret);

		k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work,
					  K_SECONDS(m_config.scan_interval));

		k_mutex_unlock(&m_scan_lock);

		return;
	}

	k_work_schedule_for_queue(&m_scan_work_q, &m_stop_scan_work,
				  K_SECONDS(m_config.scan_duration));

	LOG_DBG("Scan started");
}

static void stop_scan_work_handler(struct k_work *work)
{
	int ret;

	LOG_DBG("Stopping scan...");

	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
	}

	k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work,
				  K_SECONDS(m_config.scan_interval));

	LOG_DBG("Scan stopped");

	k_mutex_unlock(&m_scan_lock);
}

int ctr_ble_tag_is_addr_empty(const uint8_t addr[BT_ADDR_SIZE])
{
	static const uint8_t empty_addr[BT_ADDR_SIZE] = {0};
	return memcmp(addr, empty_addr, BT_ADDR_SIZE) == 0;
}

int ctr_ble_tag_read(size_t slot, uint8_t addr[BT_ADDR_SIZE], int *rssi, float *voltage,
		     float *temperature, float *humidity, bool *valid)
{
	if (slot < 0 || slot >= CTR_BLE_TAG_COUNT) {
		return -EINVAL;
	}

	if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
		return -ENOENT;
	}

	k_mutex_lock(&m_tag_data_lock, K_FOREVER);

	uint64_t now = k_uptime_get();

	if (m_tag_data[slot].valid &&
	    (now > m_tag_data[slot].timestamp +
			   m_config.scan_interval * 1000 * CTR_BLE_TAG_DATA_TIMEOUT_INTERVALS)) {
		m_tag_data[slot].valid = false;
	}

	if (addr) {
		sys_memcpy_swap(addr, m_config.addr[slot], BT_ADDR_SIZE);
	}

	if (rssi) {
		*rssi = m_tag_data[slot].valid ? m_tag_data[slot].rssi : INT_MAX;
	}

	if (voltage) {
		*voltage = m_tag_data[slot].valid ? m_tag_data[slot].voltage / 1000.f : NAN;
	}

	if (temperature) {
		*temperature = m_tag_data[slot].valid ? m_tag_data[slot].temperature : NAN;
	}

	if (humidity) {
		*humidity = m_tag_data[slot].valid ? m_tag_data[slot].humidity : NAN;
	}

	*valid = m_tag_data[slot].valid;

	k_mutex_unlock(&m_tag_data_lock);

	return 0;
}

static int h_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	int ret;
	const char *next;

#define SETTINGS_SET(_key, _var, _size)                                                            \
	do {                                                                                       \
		if (settings_name_steq(key, _key, &next) && !next) {                               \
			if (len != _size) {                                                        \
				return -EINVAL;                                                    \
			}                                                                          \
			ret = read_cb(cb_arg, _var, len);                                          \
			if (ret < 0) {                                                             \
				LOG_ERR("Call `read_cb` failed: %d", ret);                         \
				return ret;                                                        \
			}                                                                          \
			return 0;                                                                  \
		}                                                                                  \
	} while (0)

	SETTINGS_SET("enabled", &m_config_interim.enabled, sizeof(m_config_interim.enabled));
	SETTINGS_SET("scan-interval", &m_config_interim.scan_interval,
		     sizeof(m_config_interim.scan_interval));
	SETTINGS_SET("scan-duration", &m_config_interim.scan_duration,
		     sizeof(m_config_interim.scan_duration));
	SETTINGS_SET("addr-0", m_config_interim.addr[0], sizeof(m_config_interim.addr[0]));
	SETTINGS_SET("addr-1", m_config_interim.addr[1], sizeof(m_config_interim.addr[1]));
	SETTINGS_SET("addr-2", m_config_interim.addr[2], sizeof(m_config_interim.addr[2]));
	SETTINGS_SET("addr-3", m_config_interim.addr[3], sizeof(m_config_interim.addr[3]));
	SETTINGS_SET("addr-4", m_config_interim.addr[4], sizeof(m_config_interim.addr[4]));
	SETTINGS_SET("addr-5", m_config_interim.addr[5], sizeof(m_config_interim.addr[5]));
	SETTINGS_SET("addr-6", m_config_interim.addr[6], sizeof(m_config_interim.addr[6]));
	SETTINGS_SET("addr-7", m_config_interim.addr[7], sizeof(m_config_interim.addr[7]));

#undef SETTINGS_SET

	return 0;
}

static int h_commit(void)
{
	LOG_DBG("Loaded settings in full");
	memcpy(&m_config, &m_config_interim, sizeof(m_config));
	return 0;
}

static int h_export(int (*export_func)(const char *name, const void *val, size_t val_len))
{
#define EXPORT_FUNC(_key, _var, _size)                                                             \
	do {                                                                                       \
		(void)export_func(SETTINGS_PFX "/" _key, _var, _size);                             \
	} while (0)

	EXPORT_FUNC("enabled", &m_config_interim.enabled, sizeof(m_config_interim.enabled));
	EXPORT_FUNC("scan-interval", &m_config_interim.scan_interval,
		    sizeof(m_config_interim.scan_interval));
	EXPORT_FUNC("scan-duration", &m_config_interim.scan_duration,
		    sizeof(m_config_interim.scan_duration));
	EXPORT_FUNC("addr-0", m_config_interim.addr[0], sizeof(m_config_interim.addr[0]));
	EXPORT_FUNC("addr-1", m_config_interim.addr[1], sizeof(m_config_interim.addr[1]));
	EXPORT_FUNC("addr-2", m_config_interim.addr[2], sizeof(m_config_interim.addr[2]));
	EXPORT_FUNC("addr-3", m_config_interim.addr[3], sizeof(m_config_interim.addr[3]));
	EXPORT_FUNC("addr-4", m_config_interim.addr[4], sizeof(m_config_interim.addr[4]));
	EXPORT_FUNC("addr-5", m_config_interim.addr[5], sizeof(m_config_interim.addr[5]));
	EXPORT_FUNC("addr-6", m_config_interim.addr[6], sizeof(m_config_interim.addr[6]));
	EXPORT_FUNC("addr-7", m_config_interim.addr[7], sizeof(m_config_interim.addr[7]));

#undef EXPORT_FUNC

	return 0;
}

static void print_enabled(const struct shell *shell)
{
	const char *state = m_config_interim.enabled ? "true" : "false";

	shell_print(shell, "tag config enabled %s", state);
}

static void print_scan_interval(const struct shell *shell)
{
	shell_print(shell, "tag config scan-interval %d", m_config_interim.scan_interval);
}

static void print_scan_duration(const struct shell *shell)
{
	shell_print(shell, "tag config scan-duration %d", m_config_interim.scan_duration);
}

static void print_tag_list(const struct shell *shell)
{
	int ret;

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		uint8_t swap_addr[BT_ADDR_SIZE];
		char addr_str[BT_ADDR_SIZE * 2 + 1];

		sys_memcpy_swap(swap_addr, m_config_interim.addr[slot], BT_ADDR_SIZE);

		ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, addr_str, BT_ADDR_SIZE * 2 + 1, false);
		if (ret < 0) {
			LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
			return;
		}

		shell_print(shell, "tag config devices addr %d %s", slot, addr_str);
	}
}

static int cmd_config_enabled(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_enabled(shell);
		shell_print(shell, "command succeeded");
		return 0;
	}

	if (argc == 2) {
		if (strcmp(argv[1], "true") == 0) {
			m_config_interim.enabled = true;
			shell_print(shell, "command succeeded");
			return 0;
		} else if (strcmp(argv[1], "false") == 0) {
			m_config_interim.enabled = false;
			shell_print(shell, "command succeeded");
			return 0;
		}

		shell_print(shell, "invalid input");
		shell_error(shell, "command failed");

		return -EINVAL;
	}

	shell_error(shell, "command failed");
	shell_help(shell);

	return -EINVAL;
}

static int cmd_config_scan_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_scan_interval(shell);
		shell_print(shell, "command succeeded");
		return 0;
	}

	if (argc == 2) {
		long val = strtol(argv[1], NULL, 10);

		if (val < 1 || val > 86400) {
			shell_print(shell, "invalid input");
			shell_error(shell, "command failed");
			return -EINVAL;
		}

		m_config_interim.scan_interval = val;

		shell_print(shell, "command succeeded");

		return 0;
	}

	shell_error(shell, "command failed");
	shell_help(shell);

	return -EINVAL;
}

static int cmd_config_scan_duration(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_scan_duration(shell);
		shell_print(shell, "command succeeded");
		return 0;
	}

	if (argc == 2) {
		long val = strtol(argv[1], NULL, 10);

		if (val < 1 || val > 86400) {
			shell_print(shell, "invalid input");
			shell_error(shell, "command failed");
			return -EINVAL;
		}

		m_config_interim.scan_duration = val;

		shell_print(shell, "command succeeded");

		return 0;
	}

	shell_error(shell, "command failed");
	shell_help(shell);

	return -EINVAL;
}

static int cmd_config_add_tag(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_print(shell, "unknown parameter: %s", argv[1]);
		shell_error(shell, "command failed");
		shell_help(shell);
		return -EINVAL;
	}

	uint8_t addr[BT_ADDR_SIZE];

	ret = ctr_hex2buf(argv[1], addr, BT_ADDR_SIZE, false);
	if (ret < 0) {
		LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
		shell_print(shell, "invalid input");
		shell_error(shell, "command failed");
		return -EINVAL;
	}

	sys_mem_swap(addr, BT_ADDR_SIZE);

	int8_t empty_slot = -1;

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (memcmp(m_config_interim.addr[slot], addr, BT_ADDR_SIZE) == 0) {
			shell_print(shell, "tag addr %s already exists", argv[1]);
			shell_error(shell, "command failed");
			return -EEXIST;
		}

		if (empty_slot == -1 && ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
			empty_slot = slot;
			break;
		}
	}

	if (empty_slot != -1) {
		memcpy(m_config_interim.addr[empty_slot], addr, BT_ADDR_SIZE);
		shell_print(shell, "tag addr %s added to slot %d", argv[1], empty_slot);
		shell_print(shell, "command succeeded");
		return 0;
	}

	shell_print(shell, "no slot available");
	shell_error(shell, "command failed");

	return -ENOSPC;
}

static int cmd_config_remove_tag(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_print(shell, "unknown parameter: %s", argv[1]);
		shell_error(shell, "command failed");
		shell_help(shell);
		return -EINVAL;
	}

	if (strlen(argv[1]) != 1 || !isdigit(argv[1][0])) {
		uint8_t addr[BT_ADDR_SIZE];

		ret = ctr_hex2buf(argv[1], addr, BT_ADDR_SIZE, false);
		if (ret < 0) {
			LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
			shell_print(shell, "invalid address format");
			shell_error(shell, "command failed");
			return ret;
		}

		for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
			if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
				continue;
			}

			if (!memcmp(m_config_interim.addr[slot], addr, BT_ADDR_SIZE)) {
				memset(m_config_interim.addr[slot], 0, BT_ADDR_SIZE);
				shell_print(shell, "tag addr %s removed", argv[1]);
				shell_print(shell, "command succeeded");
				return 0;
			}
		}

		shell_print(shell, "tag addr %s not found", argv[1]);
		shell_error(shell, "command failed");

		return -ENOENT;
	} else {
		int slot = strtol(argv[1], NULL, 10);
		if (slot < 0 || slot >= CTR_BLE_TAG_COUNT) {
			shell_print(shell, "slot %d out of range", slot);
			shell_error(shell, "command failed");
			return -EINVAL;
		}

		if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
			shell_print(shell, "slot %d not found", slot);
			shell_error(shell, "command failed");
			return -ENOENT;
		}

		memset(m_config_interim.addr[slot], 0, BT_ADDR_SIZE);
		shell_print(shell, "slot %d removed", slot);
		shell_print(shell, "command succeeded");

		return 0;
	}
}

static int cmd_config_list_tags(const struct shell *shell, size_t argc, char **argv)
{
	print_tag_list(shell);

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_config_clear_tags(const struct shell *shell, size_t argc, char **argv)
{
	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		memset(m_config_interim.addr[slot], 0, BT_ADDR_SIZE);
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_scan(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!m_config.enabled) {
		shell_print(shell, "tag functionality is disabled");
		shell_error(shell, "command aborted");
		return -EPERM;
	}

	for (int i = 30; i; i--) {
		ret = k_mutex_lock(&m_scan_lock, K_NO_WAIT);
		if (!ret) {
			break;
		}

		if (i == 1) {
			shell_print(shell, "waiting timed out");
			shell_error(shell, "command failed");
			return -EBUSY;

		} else {
			shell_print(shell, "waiting for scan opportunity...");
		}

		k_sleep(K_SECONDS(2));
	}

	shell_print(shell, "scanning...");

	struct bt_le_scan_param param = SCAN_PARAMS_DEFAULTS;

	ret = bt_le_scan_start(&param, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed: %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		return ret;
	}

	k_sleep(K_SECONDS(CTR_BLE_TAG_ENROLL_TIMEOUT_SEC));

	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		return ret;
	}

	k_mutex_unlock(&m_scan_lock);

	shell_print(shell, "scan finished");

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		uint8_t addr[BT_ADDR_SIZE];
		int rssi;
		float voltage;
		float temperature;
		float humidity;
		bool valid;

		ret = ctr_ble_tag_read(slot, addr, &rssi, &voltage, &temperature, &humidity,
				       &valid);
		if (ret) {
			continue;
		}

		if (!valid) {
			LOG_DBG("Data was invalid");
			continue;
		}

		char addr_str[BT_ADDR_SIZE * 2 + 1];

		ret = ctr_buf2hex(addr, BT_ADDR_SIZE, addr_str, sizeof(addr_str), false);
		if (ret < 0) {
			LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell,
			    "slot %u: addr: %s / rssi: %d dBm / voltage: %.2f V / "
			    "temperature: %.2f C / "
			    "humidity: %.2f %%",
			    slot, addr_str, rssi, voltage, temperature, humidity);
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_show(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!m_config.enabled) {
		shell_print(shell, "tag subsystem is disabled");
		shell_error(shell, "command aborted");
		return -EPERM;
	}

	shell_print(shell, "cached data:");

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		uint8_t addr[BT_ADDR_SIZE];
		int rssi;
		float voltage;
		float temperature;
		float humidity;
		bool valid;

		ret = ctr_ble_tag_read(slot, addr, &rssi, &voltage, &temperature, &humidity,
				       &valid);
		if (ret) {
			continue;
		}

		if (!valid) {
			LOG_DBG("Data was invalid");
			continue;
		}

		char addr_str[BT_ADDR_SIZE * 2 + 1];

		ret = ctr_buf2hex(addr, BT_ADDR_SIZE, addr_str, sizeof(addr_str), false);
		if (ret < 0) {
			LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell,
			    "slot %u: addr: %s / rssi: %d dBm / voltage: %.2f V / "
			    "temperature: %.2f C / "
			    "humidity: %.2f %%",
			    slot, addr_str, rssi, voltage, temperature, humidity);
	}

	shell_print(shell, "command succeeded");

	return 0;
}

static int cmd_enroll(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (!m_config.enabled) {
		shell_print(shell, "tag subsystem is disabled");
		shell_error(shell, "command aborted");
		return 0;
	}

	bool can_enroll = false;

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[slot])) {
			can_enroll = true;
		}
	}

	if (!can_enroll) {
		shell_print(shell, "no slot available for enrollment");
		shell_error(shell, "command failed");
		return -ENOSPC;
	}

	for (int i = 30; i; i--) {
		ret = k_mutex_lock(&m_scan_lock, K_NO_WAIT);
		if (!ret) {
			break;
		}

		if (i == 1) {
			shell_print(shell, "waiting timed out");
			shell_error(shell, "command failed");
			return -EBUSY;

		} else {
			shell_print(shell, "waiting for enrollment opportunity...");
		}

		k_sleep(K_SECONDS(2));
	}

	shell_print(shell, "enrolling...");

	struct bt_le_scan_param param = SCAN_PARAMS_DEFAULTS;
	param.options = BT_LE_SCAN_OPT_NONE;

	/* Start scan without filter */
	ret = bt_le_scan_start(&param, enroll_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		return ret;
	}

	ret = k_sem_take(&m_has_enrolled_sem, K_SECONDS(CTR_BLE_TAG_ENROLL_TIMEOUT_SEC));
	if (!ret) {
		shell_print(shell, "enrolled addr: %s", m_enroll_addr_str);
	} else if (ret == -EAGAIN) {
		shell_print(shell, "enrollment timed out");
	} else {
		LOG_ERR("Call `k_sem_take` failed %d", ret);
	}

	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Call `bt_le_scan_stop` failed %d", ret);
		shell_error(shell, "command failed");
		k_mutex_unlock(&m_scan_lock);
		return ret;
	}

	k_mutex_unlock(&m_scan_lock);

	shell_print(shell, "command succeeded");

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_enabled(shell);
	print_scan_interval(shell);
	print_scan_duration(shell);
	print_tag_list(shell);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag_config_devices,

	SHELL_CMD_ARG(add, NULL, "Add a device.", cmd_config_add_tag, 2, 0),
	SHELL_CMD_ARG(list, NULL, "List all devices.", cmd_config_list_tags, 1, 0),
	SHELL_CMD_ARG(remove, NULL, "Remove a device.", cmd_config_remove_tag, 2, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear all devices.", cmd_config_clear_tags, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag_config,
	SHELL_CMD_ARG(show, NULL, "List current configuration.",cmd_config_show, 1, 0),

	SHELL_CMD_ARG(enabled, NULL, "Enable or disable the BLE tag scanner.", cmd_config_enabled, 1, 1),
	SHELL_CMD_ARG(scan-interval, NULL, "Set the BLE tag scanner scan interval (seconds).", cmd_config_scan_interval, 1, 1),
	SHELL_CMD_ARG(scan-duration, NULL, "Set the BLE tag scanner scan duration (seconds).", cmd_config_scan_duration, 1, 1),

	SHELL_CMD_ARG(devices, &sub_tag_config_devices, "BLE tag scanner device commands.", print_help, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_tag,

	SHELL_CMD_ARG(config, &sub_tag_config,
				  "Configuration commands.",
				  print_help, 1, 0),
	SHELL_CMD_ARG(enroll, NULL, "Enroll a device nearby (12 seconds).", cmd_enroll, 1, 0),
	SHELL_CMD_ARG(read, NULL, "Read enrolled devices (12 seconds).", cmd_scan, 1, 0),
	SHELL_CMD_ARG(show, NULL, "Show cached readings of enrolled devices.", cmd_show, 1, 0),

	SHELL_SUBCMD_SET_END
);

/* clang-format on */

SHELL_CMD_REGISTER(tag, &sub_tag, "BLE tag commands.", print_help);

static int init(void)
{
	int ret;

	LOG_INF("System initialization");

	static struct settings_handler sh = {
		.name = SETTINGS_PFX,
		.h_set = h_set,
		.h_commit = h_commit,
		.h_export = h_export,
	};

	ret = settings_register(&sh);
	if (ret) {
		LOG_ERR("Call `settings_register` failed: %d", ret);
		return ret;
	}

	ret = settings_load_subtree(SETTINGS_PFX);
	if (ret) {
		LOG_ERR("Call `settings_load_subtree` failed: %d", ret);
		return ret;
	}

	ctr_config_append_show(SETTINGS_PFX, cmd_config_show);

	ret = settings_load();
	if (ret) {
		LOG_ERR("Call `settings_load` failed: %d", ret);
		return ret;
	}

	ret = bt_le_filter_accept_list_clear();
	if (ret) {
		LOG_ERR("Call `bt_le_filter_accept_list_clear` failed: %d", ret);
		return ret;
	}

	for (size_t slot = 0; slot < CTR_BLE_TAG_COUNT; slot++) {
		if (ctr_ble_tag_is_addr_empty(m_config.addr[slot])) {
			continue;
		}

		bt_addr_le_t addr = {
			.type = BT_ADDR_LE_PUBLIC,
		};

		memcpy(addr.a.val, m_config.addr[slot], BT_ADDR_SIZE);

		ret = bt_le_filter_accept_list_add((const bt_addr_le_t *)&addr);
		if (ret) {
			LOG_ERR("Call `bt_le_filter_accept_list_add` failed: %d", ret);
			return ret;
		}
	}

	if (m_config.enabled) {
		k_work_queue_start(&m_scan_work_q, m_scan_work_q_stack,
				   K_THREAD_STACK_SIZEOF(m_scan_work_q_stack),
				   K_LOWEST_APPLICATION_THREAD_PRIO, NULL);

		k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work, K_NO_WAIT);
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
