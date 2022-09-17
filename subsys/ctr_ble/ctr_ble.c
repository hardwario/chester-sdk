/* CHESTER includes */
#include <chester/ctr_config.h>
#include <chester/ctr_info.h>

/* Zephyr includes */
#include <bluetooth/services/dfu_smp.h>
#include <bluetooth/services/nus.h>
#include <img_mgmt/img_mgmt.h>
#include <os_mgmt/os_mgmt.h>
#include <shell_mgmt/shell_mgmt.h>
#include <shell/shell_bt_nus.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/smp_bt.h>
#include <zephyr/settings/settings.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/zephyr.h>

/* Standard includes */
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(ctr_ble, CONFIG_CTR_BLE_LOG_LEVEL);

#define SETTINGS_PFX "ble"

#define TX_POWER_DBM_ADV 8
#define TX_POWER_DBM_CONN 8

struct config {
	char passkey[6 + 1];
};

static struct config m_config_interim;
static struct config m_config;

/* clang-format off */
#define ADV_OPT                                                                                    \
	BT_LE_ADV_OPT_CONNECTABLE |                                                                \
	BT_LE_ADV_OPT_SCANNABLE |                                                                  \
	BT_LE_ADV_OPT_USE_NAME |                                                                   \
	BT_LE_ADV_OPT_FORCE_NAME_IN_AD
/* clang-format on */

static const struct bt_data m_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data m_sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_DFU_SMP_SERVICE_VAL),
};

static struct bt_conn *m_current_conn;

static int set_device_name(void)
{
	int ret;

	char name[CONFIG_BT_DEVICE_NAME_MAX + 1];

	char *serial_number;
	ret = ctr_info_get_serial_number(&serial_number);
	if (ret) {
		LOG_WRN("Call `ctr_info_get_serial_number` failed: %d", ret);
		return ret;
	}

	ret = snprintf(name, sizeof(name), "%s %s", CONFIG_BT_DEVICE_NAME, serial_number);
	if (ret < 0) {
		LOG_ERR("Call `snprintf` failed: %d", ret);
		return ret;
	}

	ret = bt_set_name(name);
	if (ret) {
		LOG_ERR("Call `bt_set_name` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int load_passkey(void)
{
	int ret;

	char *ble_passkey;
	ret = ctr_info_get_ble_passkey(&ble_passkey);
	if (ret) {
		LOG_WRN("Call `ctr_info_get_ble_passkey` failed: %d", ret);
		ble_passkey = NULL;
	}

	unsigned long passkey;

	if (strlen(m_config.passkey)) {
		passkey = strtoul(m_config.passkey, NULL, 10);
	} else if (ble_passkey) {
		passkey = strtoul(ble_passkey, NULL, 10);
	} else {
		passkey = 0;
	}

	if (passkey < 0 || passkey > 999999) {
		LOG_ERR("Invalid passkey: %lu", passkey);
		return -EINVAL;
	}

	ret = bt_passkey_set(passkey);
	if (ret) {
		LOG_ERR("Call `bt_passkey_set` failed: %d", ret);
		return ret;
	}

	LOG_DBG("Set passkey: %06lu", passkey);

	return 0;
}

static int set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_power_level)
{
	int ret;

	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct net_buf *buf;
	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp));
	if (!buf) {
		LOG_ERR("Call `bt_hci_cmd_create` failed");
		return -EIO;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle_type = handle_type;
	cp->handle = sys_cpu_to_le16(handle);
	cp->tx_power_level = tx_power_level;

	struct net_buf *rsp;
	ret = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
	if (ret) {
		LOG_ERR("Call `bt_hci_cmd_send_sync` failed: %d", ret);
		return ret;
	}

	struct bt_hci_rp_vs_write_tx_power_level *rp = (void *)rsp->data;
	LOG_INF("Handle type: %u; Handle: %u; Requested: %d dBm; Selected: %d dBm\n", handle_type,
	        handle, tx_power_level, rp->selected_tx_power);

	net_buf_unref(rsp);

	return 0;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	int ret;

	if (err) {
		LOG_ERR("Connection failed (reason: %u)", err);
		return;
	}

	LOG_INF("Connected");

	m_current_conn = bt_conn_ref(conn);

	ret = bt_conn_set_security(conn, BT_SECURITY_L4);
	if (ret) {
		LOG_ERR("Call `bt_conn_set_security` failed: %d", ret);
	}

#if defined(CONFIG_SHELL_BT_NUS)
	shell_bt_nus_enable(conn);
#endif /* defined(CONFIG_SHELL_BT_NUS) */
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason: %u)", reason);

#if defined(CONFIG_SHELL_BT_NUS)
	shell_bt_nus_disable();
#endif /* defined(CONFIG_SHELL_BT_NUS) */

	if (m_current_conn) {
		bt_conn_unref(m_current_conn);
		m_current_conn = NULL;
	}
}

BT_CONN_CB_DEFINE(conn_cb) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb = {
	.cancel = auth_cancel,
};

static void auth_pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s (bonded: %d)", addr, bonded);
}

static void auth_pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing failed: %s (reason: %d)", addr, reason);

	bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
}

static struct bt_conn_auth_info_cb auth_info_cb = {
	.pairing_complete = auth_pairing_complete,
	.pairing_failed = auth_pairing_failed,
};

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

	SETTINGS_SET("passkey", m_config_interim.passkey, sizeof(m_config_interim.passkey));

#undef SETTINGS_SET

	return -ENOENT;
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

	EXPORT_FUNC("passkey", m_config_interim.passkey, sizeof(m_config_interim.passkey));

#undef EXPORT_FUNC

	return 0;
}

static void print_passkey(const struct shell *shell)
{
	shell_print(shell, SETTINGS_PFX " config passkey %s", m_config_interim.passkey);
}

static int cmd_config_show(const struct shell *shell, size_t argc, char **argv)
{
	print_passkey(shell);

	return 0;
}

static int cmd_config_passkey(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		print_passkey(shell);
		return 0;
	}

	if (argc == 2) {
		size_t len = strlen(argv[1]);

		if (len && len != 6) {
			shell_error(shell, "invalid format");
			return -EINVAL;
		}

		for (size_t i = 0; i < len; i++) {
			if (!isdigit((int)argv[1][i])) {
				shell_error(shell, "invalid format");
				return -EINVAL;
			}
		}

		strcpy(m_config_interim.passkey, argv[1]);
		return 0;
	}

	shell_help(shell);
	return -EINVAL;
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

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ble_config,

	SHELL_CMD_ARG(show, NULL,
	              "List current configuration.",
	              cmd_config_show, 1, 0),

	SHELL_CMD_ARG(passkey, NULL,
	              "Get/Set BLE passkey (format: <empty or 6 digits).",
	              cmd_config_passkey, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ble,

	SHELL_CMD_ARG(config, &sub_ble_config,
	              "Configuration commands.",
	              print_help, 1, 0),

	SHELL_SUBCMD_SET_END
);

/* clang-format on */

SHELL_CMD_REGISTER(ble, &sub_ble, "BLE commands.", print_help);

static int init(const struct device *dev)
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

#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
	os_mgmt_register_group();
#endif

#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
	img_mgmt_register_group();
#endif

#ifdef CONFIG_MCUMGR_CMD_SHELL_MGMT
	shell_mgmt_register_group();
#endif

	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("Call `bt_enable` failed: %d", ret);
		return ret;
	}

	ret = settings_load();
	if (ret) {
		LOG_ERR("Call `settings_load` failed: %d", ret);
		return ret;
	}

	ret = bt_conn_auth_cb_register(&auth_cb);
	if (ret) {
		LOG_ERR("Call `bt_conn_auth_cb_register` failed: %d", ret);
		return ret;
	}

	ret = bt_conn_auth_info_cb_register(&auth_info_cb);
	if (ret) {
		LOG_ERR("Call `bt_conn_auth_info_cb_register` failed: %d", ret);
		return ret;
	}

	ret = set_device_name();
	if (ret) {
		LOG_WRN("Call `set_device_name` failed: %d", ret);
	}

	ret = load_passkey();
	if (ret) {
		LOG_WRN("Call `load_passkey` failed: %d", ret);
	}

#if defined(CONFIG_MCUMGR_SMP_BT)
	ret = smp_bt_register();
	if (ret) {
		LOG_ERR("Call `smp_bt_register` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_MCUMGR_SMP_BT) */

#if defined(CONFIG_SHELL_BT_NUS)
	ret = shell_bt_nus_init();
	if (ret) {
		LOG_ERR("Call `shell_bt_nus_init` failed: %d", ret);
		return ret;
	}
#endif /* defined(CONFIG_SHELL_BT_NUS) */

	struct bt_le_adv_param *adv_param =
	        BT_LE_ADV_PARAM(ADV_OPT, BT_GAP_ADV_SLOW_INT_MIN, BT_GAP_ADV_SLOW_INT_MAX, NULL);

	ret = bt_le_adv_start(adv_param, m_ad, ARRAY_SIZE(m_ad), m_sd, ARRAY_SIZE(m_sd));
	if (ret) {
		LOG_ERR("Call `bt_le_adv_start` failed: %d", ret);
		return ret;
	}

	ret = set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, TX_POWER_DBM_ADV);
	if (ret) {
		LOG_ERR("Call `set_tx_power` (BT_HCI_VS_LL_HANDLE_TYPE_ADV) failed: %d", ret);
		return ret;
	}

	ret = set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, 0, TX_POWER_DBM_CONN);
	if (ret) {
		LOG_ERR("Call `set_tx_power` (BT_HCI_VS_LL_HANDLE_TYPE_CONN) failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
