#include <ctr_info.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/services/nus.h>
#include <bluetooth/uuid.h>
#include <img_mgmt/img_mgmt.h>
#include <init.h>
#include <logging/log.h>
#include <mgmt/mcumgr/smp_bt.h>
#include <os_mgmt/os_mgmt.h>
#include <shell/shell_bt_nus.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(ctr_ble, CONFIG_CTR_BLE_LOG_LEVEL);

static const struct bt_data ad_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static int set_device_name(void)
{
	int ret;

	static char bt_dev_name[CONFIG_BT_DEVICE_NAME_MAX];

	uint32_t sn;
	ret = ctr_info_get_serial_number(&sn);
	if (ret) {
		LOG_WRN("Failed to get serial number: %d", ret);

		/*
		 * The default device name is CONFIG_BT_DEVICE_NAME. Therefore
		 * we can safely return here if we were not able to acquire a
		 * serial number.
		 */
		return ret;
	}

	ret = snprintf(bt_dev_name, sizeof(bt_dev_name), "%s %u", CONFIG_BT_DEVICE_NAME, sn);
	if (ret < 0) {
		LOG_ERR("Failed to generate device name: %d", ret);
		return ret;
	}

	ret = bt_set_name(bt_dev_name);
	if (ret) {
		LOG_ERR("Failed to set Bluetooth device name: %d", ret);
		return ret;
	}

	return 0;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	LOG_INF("Connected");

#ifdef CONFIG_SHELL_BT_NUS
	shell_bt_nus_enable(conn);
#endif /* CONFIG_SHELL_BT_NUS */
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);

#ifdef CONFIG_SHELL_BT_NUS
	shell_bt_nus_disable();
#endif /* CONFIG_SHELL_BT_NUS */
}

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	static struct bt_conn_cb conn_callbacks = {
		.connected = connected,
		.disconnected = disconnected,
	};

	bt_conn_cb_register(&conn_callbacks);

	ret = bt_enable(NULL);
	if (ret < 0) {
		LOG_ERR("Call `bt_enable` failed: %d", ret);
		return ret;
	}

	ret = set_device_name();
	if (ret < 0) {
		LOG_WRN("Call `set_device_name` failed: %d", ret);
	}

	os_mgmt_register_group();
	img_mgmt_register_group();

#ifdef CONFIG_SHELL_BT_NUS
	ret = shell_bt_nus_init();
	if (ret < 0) {
		LOG_ERR("Call `shell_bt_nus_init` failed: %d", ret);
		return ret;
	}
#endif /* CONFIG_SHELL_BT_NUS */

	ret = smp_bt_register();
	if (ret < 0) {
		LOG_ERR("Call `smp_bt_register` failed: %d", ret);
		return ret;
	}

	LOG_INF("Bluetooth device name: %s", log_strdup(bt_get_name()));

	ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad_data, ARRAY_SIZE(ad_data), NULL, 0);
	if (ret < 0) {
		LOG_ERR("Call `bt_le_adv_start` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
