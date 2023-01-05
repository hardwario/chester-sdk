/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/reboot.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Chester includes */
#include <chester/ctr_sat.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void message_processed_callback(ctr_sat_message_event* event_data, void* user_data) {
	LOG_INF("Message was successfully transmitted and confirmed.");
}

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_sat_start();
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_start` failed: %d", ret);
		
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

	ret = ctr_sat_setup_wifi_devkit("Ekdigkwkbdheid", "uciejsigkridojgjeudjrmdkshvf", "pb58qqh85LO2O6N8Px54VVQbXO7wiJ4r0kMgq6h1opuCa1Ac2E7sTJj8oRaPP6vJ0trv78vWSDFo77yZbtbjvPFowOo6akdg");
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_setup_wifi_devkit` failed: %d", ret);

		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}
	
	k_sleep(K_MSEC(5000));

	ret = ctr_sat_revoke_all_messages();
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_revoke_all_messages` failed: %d", ret);

		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}
	
	// calling revoke all twice for testing internal error handlers
	ret = ctr_sat_revoke_all_messages();
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_revoke_all_messages` failed: %d", ret);

		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

	ret = ctr_sat_send_message("ahoj", 4, message_processed_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_send_message` failed: %d", ret);

		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}
	
	LOG_INF("Everything good. It is time to sleep :)");

	for (;;) {
		k_sleep(K_FOREVER);
	}
}
