/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/ring_buffer.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Chester includes */
// #include "../samples/chester_sat/wifi_credentials.h"

#include <chester/ctr_led.h>
#include <chester/ctr_sat.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static message_handle msg1;
static message_handle msg2;

volatile message_handle sent_events[5];
volatile int events = 0;

void message_processed_callback(enum ctr_sat_event event, union ctr_sat_event_data *data,
				void *user_data)
{

	if (event == CTR_SAT_EVENT_MESSAGE_SENT && events < 5) {
		sent_events[events++] = data->msg_send.msg;
	}

	LOG_INF("Message was successfully transmitted and confirmed.");
}

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	struct ctr_sat sat;

	ctr_led_set(CTR_LED_CHANNEL_R, false);
	ctr_led_set(CTR_LED_CHANNEL_G, false);

	ctr_led_set(CTR_LED_CHANNEL_Y, true);
	k_sleep(K_MSEC(30000));
	ctr_led_set(CTR_LED_CHANNEL_Y, false);

	const struct ctr_sat_hwcfg sat_hwcfg = CTR_SAT_HWCONFIG_SYSCON;
	// ctr_sat_get_hwcfg_syscon(&sat_hwcfg);

	// const struct device *uart_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_sc16is740_syscon));
	// const struct device *gpio_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_tca9534a_syscon));

	// const struct device *dev_uart = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_sc16is740));
	// const struct device *dev_uart = DEVICE_DT_GET(DT_NODELABEL(ctr_x2_sc16is740_a));

	// const struct device *dev_gpio_wakeup = DEVICE_DT_GET(DT_NODELABEL(ctr_v1_sc16is740));

	ret = ctr_sat_start(&sat, &sat_hwcfg);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_start` failed: %d", ret);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

	ret = ctr_sat_set_callback(&sat, message_processed_callback, NULL);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_set_callback` failed: %d", ret);
	}

	// ret = ctr_sat_use_wifi_devkit(&sat, WIFI_SSID, WIFI_PASS, WIFI_API_KEY);
	// if (ret < 0) {
	//	LOG_ERR("Call `ctr_sat_use_wifi_devkit` failed: %d", ret);
	//
	//	ctr_led_set(CTR_LED_CHANNEL_R, true);
	//	k_sleep(K_MSEC(5000));
	//	sys_reboot(SYS_REBOOT_COLD);
	//}

	// k_sleep(K_MSEC(5000));

	/*
		ret = ctr_sat_flush_messages(&sat);
		if (ret < 0) {
			LOG_ERR("Call `ctr_sat_flush_messages` failed: %d", ret);

			ctr_led_set(CTR_LED_CHANNEL_R, true);
			k_sleep(K_MSEC(5000));
			sys_reboot(SYS_REBOOT_COLD);
		}
	*/

	// calling revoke all twice for testing internal error handlers
	/*
	ret = ctr_sat_flush_messages(&sat);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_flush_messages` failed: %d", ret);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}
	*/

	const char *message = "Chester say hello!";
	ret = ctr_sat_send_message(&sat, &msg1, message, strlen(message));
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_send_message` failed: %d", ret);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

	message = "Chester say hello again!";
	ret = ctr_sat_send_message(&sat, &msg2, message, strlen(message));
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_send_message` failed: %d", ret);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

	LOG_INF("Everything good. It is time to sleep :)");
	ctr_led_set(CTR_LED_CHANNEL_G, true);

	k_sleep(K_SECONDS(60));

	ctr_led_set(CTR_LED_CHANNEL_G, false);

	for (;;) {
		k_sleep(K_FOREVER);
	}
}
