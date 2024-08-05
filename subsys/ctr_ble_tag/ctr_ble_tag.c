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

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_ble_tag, LOG_LEVEL_INF);

#define SETTINGS_PFX "ble_tag"

struct config {
	bool enabled;
	int scan_interval;
	int scan_duration;
	uint8_t addr[CTR_BLE_TAG_COUNT][BT_ADDR_SIZE];
};

static struct config m_config_interim = {
	.enabled = false,
	.scan_interval = 300,
	.scan_duration = 10,
};

static struct config m_config;

struct ble_tag_data {
	int rssi;
	float temperature;
	float humidity;
	float voltage;
	uint64_t timestamp;
	bool valid;
};

static struct ble_tag_data tag_data[CTR_BLE_TAG_COUNT];
K_MUTEX_DEFINE(tag_data_lock);

static uint8_t m_scan_tag_mask, m_scan_tag_flags;
static bool m_scan_early_stop;

static volatile bool m_enrolling = false;
static char m_enroll_addr_str[BT_ADDR_SIZE * 2 + 1];

K_SEM_DEFINE(m_has_enrolled_sem, 0, 1);

K_SEM_DEFINE(m_scan_sem, 0, 1);

static struct k_work_q m_scan_work_q;
K_THREAD_STACK_DEFINE(m_scan_work_q_stack, 1024);

static void start_scan_work_handler(struct k_work *work);
static void stop_scan_work_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(m_start_scan_work, start_scan_work_handler);
static K_WORK_DELAYABLE_DEFINE(m_stop_scan_work, stop_scan_work_handler);

static struct bt_le_scan_param m_scan_params;

int ctr_ble_tag_is_addr_empty(const uint8_t addr[BT_ADDR_SIZE])
{
	static const uint8_t empty_addr[BT_ADDR_SIZE] = {0};
	return memcmp(addr, empty_addr, BT_ADDR_SIZE) == 0;
}

static int parse_data(struct net_buf_simple *buf, struct ble_tag_data *data)
{
	bool has_data = false;

	if (buf->len < 1) {
		LOG_DBG("Invalid length: %d", buf->len);
		return -EINVAL;
	}

	do {
		int len = net_buf_simple_pull_u8(buf);
		if (len > buf->len) {
			LOG_DBG("Invalid length: %d, needed: %d", buf->len, len);
			return -EINVAL;
		}

		int type = net_buf_simple_pull_u8(buf);
		if (type == 0xff) {
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

			int16_t temperature;
			uint8_t humidity;
			uint8_t voltage;

			if (data_flags & BIT(0)) {
				if (buf->len < 2) {
					LOG_DBG("Invalid length: %d, needed: %d", buf->len,
						sizeof(uint16_t));
					return -EINVAL;
				}

				temperature = net_buf_simple_pull_be16(buf);

				data->temperature = temperature / 100.0;
				LOG_DBG("Temperature: %d", temperature);
			}

			if (data_flags & BIT(1)) {
				if (buf->len < 1) {
					LOG_DBG("Invalid length: %d, needed: %d", buf->len,
						sizeof(uint8_t));
					return -EINVAL;
				}

				humidity = net_buf_simple_pull_u8(buf);

				data->humidity = humidity;
				LOG_DBG("Humidity: %d", humidity);
			}

			if (data_flags & BIT(7)) {
				if (buf->len < 1) {
					LOG_ERR("Invalid length: %d, needed: %d", buf->len,
						sizeof(uint8_t));
					return -EINVAL;
				}

				net_buf_simple_pull_mem(buf, buf->len - 1);

				voltage = net_buf_simple_pull_u8(buf);

				data->voltage = (voltage * 10) + 2000.0;
				LOG_DBG("Voltage: %d", voltage);
			}

			data->timestamp = k_uptime_get();
			has_data = true;

		} else {
			net_buf_simple_pull_mem(buf, len - 1);
		}
	} while (buf->len);

	return has_data ? 0 : -EINVAL;
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	int ret;

	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		bool same_addr = memcmp(addr->a.val, m_config_interim.addr[i], BT_ADDR_SIZE) == 0;
		bool is_empty = ctr_ble_tag_is_addr_empty(m_config_interim.addr[i]);

		bool can_enroll = m_enrolling && (rssi > CTR_BLE_TAG_ENROLL_RSSI_THRESHOLD);

		if (!same_addr && !(is_empty && can_enroll)) {
			continue;
		}

		k_mutex_lock(&tag_data_lock, K_FOREVER);

		ret = parse_data(buf, &tag_data[i]);
		if (ret) {
			k_mutex_unlock(&tag_data_lock);
			return;
		}

		if (is_empty && can_enroll) {
			memcpy(m_config_interim.addr[i], (void *)addr->a.val, BT_ADDR_SIZE);

			uint8_t swap_addr[BT_ADDR_SIZE];
			sys_memcpy_swap(swap_addr, m_config_interim.addr[i], BT_ADDR_SIZE);

			ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, m_enroll_addr_str,
					  sizeof(m_enroll_addr_str), true);
			if (ret < 0) {
				LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
				k_mutex_unlock(&tag_data_lock);
				return;
			}

			LOG_INF("Enrolled: %s", m_enroll_addr_str);
			k_sem_give(&m_has_enrolled_sem);
		}

		m_scan_tag_flags |= BIT(i);

		if (!m_enrolling && m_scan_tag_flags == m_scan_tag_mask) {
			ret = bt_le_scan_stop();
			if (ret) {
				LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
			}

			m_scan_early_stop = true;

			k_sem_give(&m_scan_sem);
		}

		tag_data[i].rssi = rssi;
		tag_data[i].valid = true;

		k_mutex_unlock(&tag_data_lock);
	}
}

int ctr_ble_tag_read(int idx, uint8_t addr[BT_ADDR_SIZE], int *rssi, float *voltage,
		     float *temperature, float *humidity, bool *valid)
{
	if (idx < 0 || idx >= CTR_BLE_TAG_COUNT ||
	    ctr_ble_tag_is_addr_empty(m_config_interim.addr[idx])) {
		return -ENOENT;
	}

	k_mutex_lock(&tag_data_lock, K_FOREVER);

	uint64_t now = k_uptime_get();

	if (tag_data[idx].valid &&
	    (now > tag_data[idx].timestamp +
			   m_config.scan_interval * 1000 * CTR_BLE_TAG_DATA_TIMEOUT_INTERVALS)) {
		tag_data[idx].valid = false;
	}

	if (addr) {
		sys_memcpy_swap(addr, m_config_interim.addr[idx], BT_ADDR_SIZE);
	}

	if (rssi) {
		if (tag_data[idx].valid) {
			*rssi = tag_data[idx].rssi;
		} else {
			*rssi = INT_MAX;
		}
	}

	if (voltage) {
		if (tag_data[idx].valid) {
			*voltage = tag_data[idx].voltage / 1000.f;
		} else {
			*voltage = NAN;
		}
	}

	if (temperature) {
		if (tag_data[idx].valid) {
			*temperature = tag_data[idx].temperature;
		} else {
			*temperature = NAN;
		}
	}

	if (humidity) {
		if (tag_data[idx].valid) {
			*humidity = tag_data[idx].humidity;
		} else {
			*humidity = NAN;
		}
	}

	*valid = tag_data[idx].valid;

	k_mutex_unlock(&tag_data_lock);

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

	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		uint8_t swap_addr[BT_ADDR_SIZE];
		char addr_str[BT_ADDR_SIZE * 2 + 1];

		sys_memcpy_swap(swap_addr, m_config_interim.addr[i], BT_ADDR_SIZE);

		ret = ctr_buf2hex(swap_addr, BT_ADDR_SIZE, addr_str, BT_ADDR_SIZE * 2 + 1, true);
		if (ret < 0) {
			LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
			return;
		}

		shell_print(shell, "tag config devices key %d %s", i, addr_str);
	}
}

static int cmd_config_enabled(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_enabled(shell);
		return 0;
	}

	if (argc == 2) {
		if (strcmp(argv[1], "true") == 0) {
			m_config_interim.enabled = true;
			return 0;
		} else if (strcmp(argv[1], "false") == 0) {
			m_config_interim.enabled = false;
			return 0;
		}

		shell_error(shell, "invalid option");

		return -EINVAL;
	}

	shell_help(shell);

	return -EINVAL;
}

static int cmd_config_scan_interval(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_scan_interval(shell);
		return 0;
	}

	if (argc == 2) {
		char *endptr;
		long val = strtol(argv[1], &endptr, 10);

		if (endptr == argv[1] || *endptr != '\0' || errno == ERANGE) {
			shell_error(shell, "invalid option");
			return -EINVAL;
		}

		if (val < 1 || val > 86400) {
			shell_error(shell, "invalid option");
			return -EINVAL;
		}

		m_config_interim.scan_interval = val;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

static int cmd_config_scan_duration(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_scan_duration(shell);
		return 0;
	}

	if (argc == 2) {
		int val = atoi(argv[1]);

		if (val < 1 || val > 86400) {
			shell_error(shell, "invalid option");
			return -EINVAL;
		}

		m_config_interim.scan_duration = val;
		return 0;
	}

	shell_help(shell);

	return -EINVAL;
}

static int cmd_config_add_tag(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	uint8_t addr[BT_ADDR_SIZE];

	ret = ctr_hex2buf(argv[1], addr, BT_ADDR_SIZE, true);
	if (ret < 0) {
		LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
		return -EINVAL;
	}

	sys_mem_swap(addr, BT_ADDR_SIZE);

	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[i])) {
			memcpy(m_config_interim.addr[i], addr, BT_ADDR_SIZE);

			m_scan_tag_mask |= BIT(i);

			return 0;
		} else if (memcmp(m_config_interim.addr[i], addr, BT_ADDR_SIZE) == 0) {
			shell_error(shell, "tag addr %s: already exists", argv[1]);
			return -EINVAL;
		}
	}

	shell_error(shell, "no more space for additional tags");
	return -ENOSPC;
}

static int cmd_config_remove_tag(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	if (argc > 2) {
		shell_error(shell, "unknown parameter: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	char *endptr;
	int index = strtol(argv[1], &endptr, 10);

	if (*endptr) {
		uint8_t addr[BT_ADDR_SIZE];

		ret = ctr_hex2buf(argv[1], addr, BT_ADDR_SIZE, true);
		if (ret < 0) {
			LOG_ERR("Call `ctr_hex2buf` failed: %d", ret);
			return -EINVAL;
		}

		sys_mem_swap(addr, BT_ADDR_SIZE);

		for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
			if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[i])) {
				continue;
			}

			if (memcmp(m_config_interim.addr[i], addr, BT_ADDR_SIZE) == 0) {
				memset(m_config_interim.addr[i], 0, BT_ADDR_SIZE);
				shell_print(shell, "tag addr: %s removed", argv[1]);

				m_scan_tag_mask &= ~BIT(i);

				return 0;
			}
		}

		shell_error(shell, "tag addr: %s not found", argv[1]);

		return -EINVAL;

	} else {
		if (index < 0 || index >= CTR_BLE_TAG_COUNT) {
			shell_error(shell, "index out of range: %d", index);
			return -EINVAL;
		}

		if (ctr_ble_tag_is_addr_empty(m_config_interim.addr[index])) {
			shell_error(shell, "tag index %d not found", index);
			return -EINVAL;
		}

		memset(m_config_interim.addr[index], 0, BT_ADDR_SIZE);
		shell_print(shell, "tag index: %d removed", index);

		m_scan_tag_mask &= ~BIT(index);

		return 0;
	}
}

static int cmd_config_list_tags(const struct shell *shell, size_t argc, char **argv)
{
	print_tag_list(shell);

	return 0;
}

static int cmd_config_clear_tags(const struct shell *shell, size_t argc, char **argv)
{
	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		memset(m_config_interim.addr[i], 0, BT_ADDR_SIZE);
	}

	m_scan_tag_mask = 0;

	return 0;
}

static int cmd_config_scan(const struct shell *shell, size_t argc, char **argv)
{
	if (!m_config.enabled) {
		shell_error(shell, "tag subsystem is disabled");
		return -EINVAL;
	}

	shell_print(shell, "scanning...");

	k_sem_take(&m_scan_sem, K_FOREVER);

	struct bt_le_scan_param scan_params = m_scan_params;

	int ret = bt_le_scan_start(&scan_params, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed (err %d)", ret);
		k_sem_give(&m_scan_sem);
		return ret;
	}

	k_sleep(K_SECONDS(CTR_BLE_TAG_ENROLL_TIMEOUT_SEC));

	if (!m_scan_early_stop) {
		ret = bt_le_scan_stop();
		if (ret) {
			LOG_ERR("Call `bt_le_scan_stop` failed (err %d)", ret);
			k_sem_give(&m_scan_sem);
			return ret;
		}
	}

	k_sem_give(&m_scan_sem);

	shell_print(shell, "scan finished");

	for (int i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		uint8_t addr[BT_ADDR_SIZE];
		int rssi;
		float voltage;
		float temperature;
		float humidity;
		bool valid;

		ret = ctr_ble_tag_read(i, addr, &rssi, &voltage, &temperature, &humidity, &valid);
		if (ret) {
			continue;
		}

		if (valid) {
			char addr_str[BT_ADDR_SIZE * 2 + 1];

			ret = ctr_buf2hex(addr, BT_ADDR_SIZE, addr_str, sizeof(addr_str), true);
			if (ret < 0) {
				LOG_ERR("Call `ctr_buf2hex` failed: %d", ret);
				return ret;
			}

			shell_print(shell,
				    "tag index %d: addr: %s / rssi: %d dBm / voltage: %.2f V / "
				    "temperature: %.2f C / "
				    "humidity: %.2f %%",
				    i, addr_str, rssi, voltage, temperature, humidity);
		}
	}

	return 0;
}

static int cmd_config_enroll(const struct shell *shell, size_t argc, char **argv)
{
	if (!m_config.enabled) {
		shell_error(shell, "tag subsystem is disabled");
		return -EINVAL;
	}

	k_sem_take(&m_scan_sem, K_FOREVER);

	shell_print(shell, "enrolling...");

	m_enrolling = true;

	struct bt_le_scan_param scan_params = m_scan_params;
	scan_params.options = BT_LE_SCAN_OPT_NONE;

	int ret = bt_le_scan_start(&scan_params, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed (err %d)", ret);
		k_sem_give(&m_scan_sem);
		return ret;
	}

	ret = k_sem_take(&m_has_enrolled_sem, K_SECONDS(CTR_BLE_TAG_ENROLL_TIMEOUT_SEC));
	if (ret) {
		shell_error(shell, "enroll timeout");
	} else {
		shell_print(shell, "enrolled addr: %s", m_enroll_addr_str);
	}

	m_enrolling = false;

	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Call `bt_le_scan_stop` failed (err %d)", ret);
		k_sem_give(&m_scan_sem);
		return ret;
	}

	k_sem_give(&m_scan_sem);

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
	SHELL_CMD_ARG(enroll, NULL, "Enroll a device nearby (10 seconds).", cmd_config_enroll, 1, 0),
	SHELL_CMD_ARG(list, NULL, "List all devices.", cmd_config_list_tags, 1, 0),
	SHELL_CMD_ARG(remove, NULL, "Remove a device.", cmd_config_remove_tag, 2, 0),
	SHELL_CMD_ARG(clear, NULL, "Clear all devices.", cmd_config_clear_tags, 1, 0),
	SHELL_CMD_ARG(read, NULL, "Read enrolled devices (10 seconds).", cmd_config_scan, 1, 0),

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

	SHELL_SUBCMD_SET_END
);

/* clang-format on */

SHELL_CMD_REGISTER(tag, &sub_tag, "BLE tag commands.", print_help);

static void start_scan_work_handler(struct k_work *work)
{
	k_sem_take(&m_scan_sem, K_FOREVER);

	int ret = bt_le_scan_start(&m_scan_params, scan_cb);
	if (ret) {
		LOG_ERR("Call `bt_le_scan_start` failed: %d", ret);
		k_sem_give(&m_scan_sem);
		return;
	}

	m_scan_tag_flags = 0;

	k_work_schedule_for_queue(&m_scan_work_q, &m_stop_scan_work,
				  K_SECONDS(m_config.scan_duration));
}

static void stop_scan_work_handler(struct k_work *work)
{
	int ret;

	if (!m_scan_early_stop) {
		ret = bt_le_scan_stop();
		if (ret) {
			LOG_ERR("Call `bt_le_scan_stop` failed: %d", ret);
			k_sem_give(&m_scan_sem);
			return;
		}
	}

	m_scan_early_stop = false;

	k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work,
				  K_SECONDS(m_config.scan_interval - m_config.scan_duration));

	k_sem_give(&m_scan_sem);
}

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

	m_scan_params.type = BT_LE_SCAN_TYPE_ACTIVE;
	m_scan_params.options = BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;
	m_scan_params.interval = CTR_BLE_TAG_SCAN_MAX_TIME;
	m_scan_params.window = CTR_BLE_TAG_SCAN_MAX_TIME;

	ret = bt_le_filter_accept_list_clear();
	if (ret) {
		LOG_ERR("Call `bt_le_filter_accept_list_clear` failed: %d", ret);
		return ret;
	}

	for (size_t i = 0; i < CTR_BLE_TAG_COUNT; i++) {
		if (!ctr_ble_tag_is_addr_empty(m_config_interim.addr[i])) {
			bt_addr_le_t addr = {
				.type = BT_ADDR_LE_PUBLIC,
			};

			memcpy(addr.a.val, m_config_interim.addr[i], BT_ADDR_SIZE);

			ret = bt_le_filter_accept_list_add((const bt_addr_le_t *)&addr);
			if (ret) {
				LOG_ERR("Call `bt_le_filter_accept_list_add` failed: %d", ret);
				return ret;
			}

			m_scan_tag_mask |= BIT(i);
		}
	}

	if (m_config.enabled) {
		k_work_queue_start(&m_scan_work_q, m_scan_work_q_stack,
				   K_THREAD_STACK_SIZEOF(m_scan_work_q_stack), K_PRIO_COOP(7),
				   NULL);

		k_work_schedule_for_queue(&m_scan_work_q, &m_start_scan_work, K_NO_WAIT);

		k_sem_give(&m_scan_sem);
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
