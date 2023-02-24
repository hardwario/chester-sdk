/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

/* Chester includes */
#include <chester/ctr_led.h>
#include <chester/ctr_sat.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static ctr_sat_msg_handle msg1;
static ctr_sat_msg_handle msg2;

void message_processed_callback(enum ctr_sat_event event, union ctr_sat_event_data *data,
				void *user_data)
{

	if (event == CTR_SAT_EVENT_MESSAGE_SENT) {
		if (data->msg_send.msg == msg1) {
			LOG_INF("Message #1 was successfully transmitted and confirmed");
		} else if (data->msg_send.msg == msg2) {
			LOG_INF("Message #2 was successfully transmitted and confirmed");
		} else {
			LOG_INF("Unknown message was transmitted and confirmed");
		}
	} else {
		LOG_HEXDUMP_INF(data->msg_recv.data, data->msg_recv.data_len,
				"Received message from cloud:");
	}
}

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	struct ctr_sat sat;

	ctr_led_set(CTR_LED_CHANNEL_R, false);
	ctr_led_set(CTR_LED_CHANNEL_G, false);

	ctr_led_set(CTR_LED_CHANNEL_Y, true);
	k_sleep(K_MSEC(30));
	ctr_led_set(CTR_LED_CHANNEL_Y, false);

	ret = ctr_sat_v1_init_w1(&sat);
	if (ret) {
		LOG_ERR("Call `ctr_sat_v1_init_w1` failed: %d", ret);
		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

	ret = ctr_sat_start(&sat);
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

	ret = ctr_sat_flush_messages(&sat);
	if (ret < 0) {
		LOG_ERR("Call `ctr_sat_flush_messages` failed: %d", ret);

		ctr_led_set(CTR_LED_CHANNEL_R, true);
		k_sleep(K_MSEC(5000));
		sys_reboot(SYS_REBOOT_COLD);
	}

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
