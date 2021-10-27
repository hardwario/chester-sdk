/* Zephyr includes */
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

LOG_MODULE_REGISTER(ctr_ble, LOG_LEVEL_DBG);

static const struct bt_data ad_data[] = { BT_DATA_BYTES(BT_DATA_FLAGS,
	                                                (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	                                  BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL) };

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	LOG_INF("Connected");

	shell_bt_nus_enable(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason %u)", reason);

	shell_bt_nus_disable();
}

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	static struct bt_conn_cb conn_callbacks = { .connected = connected,
		                                    .disconnected = disconnected };

	bt_conn_cb_register(&conn_callbacks);

	ret = bt_enable(NULL);

	if (ret < 0) {
		LOG_ERR("Call `bt_enable` failed: %d", ret);
		return ret;
	}

	ret = shell_bt_nus_init();

	if (ret < 0) {
		LOG_ERR("Call `shell_bt_nus_init` failed: %d", ret);
		return ret;
	}

	os_mgmt_register_group();
	img_mgmt_register_group();

	ret = smp_bt_register();

	if (ret < 0) {
		LOG_ERR("Call `smp_bt_register` failed: %d", ret);
		return ret;
	}

	ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad_data, ARRAY_SIZE(ad_data), NULL, 0);

	if (ret < 0) {
		LOG_ERR("Call `bt_le_adv_start` failed: %d", ret);
		return ret;
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_CTR_BLE_INIT_PRIORITY);
