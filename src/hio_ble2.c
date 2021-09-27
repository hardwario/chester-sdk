#include <hio_ble.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include <init.h>
#include <logging/log.h>
#include <zephyr.h>
#include <bluetooth/services/nus.h>
#include <img_mgmt/img_mgmt.h>
#include <mgmt/mcumgr/smp_bt.h>
#include <os_mgmt/os_mgmt.h>

#define BT_UUID_SMP_VAL BT_UUID_128_ENCODE(0x8D53DC1D, 0x1db7, 0x4cd3, 0x868b, 0x8a527460aa84)

LOG_MODULE_REGISTER(hio_ble, LOG_LEVEL_DBG);

static const struct bt_data ad_data[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static const struct bt_data sd_data[] = {
	// BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
	// BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SMP_VAL),
};

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	LOG_HEXDUMP_DBG(data, len, "bt_receive_cb");
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

static void bt_ready(int err)
{
	if (err) {
		LOG_ERR("bt_ready failed: %d", err);
	}
}

void hio_ble_init(void)
{
	int ret;

	ret = bt_enable(bt_ready);
	if (ret) {
		LOG_ERR("bt_enable failed: %d", ret);
		return;
	}

	ret = bt_nus_init(&nus_cb);
	if (ret < 0) {
		LOG_ERR("bt_nus_init failed: %d", ret);
		return;
	}

	os_mgmt_register_group();
	img_mgmt_register_group();

	ret = smp_bt_register();
	if (ret < 0) {
		LOG_ERR("smp_bt_register failed: %d", ret);
		return;
	}

	ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad_data, ARRAY_SIZE(ad_data), sd_data,
			      ARRAY_SIZE(sd_data));
	if (ret) {
		LOG_ERR("bt_le_adv_start failed: %d", ret);
		return;
	}
}
