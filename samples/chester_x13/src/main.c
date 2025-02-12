/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>
#include <chester/ctr_gpio.h>
#include <chester/drivers/ctr_x0.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/spi.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_x13_can));
/*static const struct spi_dt_spec bus =
	SPI_DT_SPEC_GET(DT_NODELABEL(ctr_x13_can), SPI_WORD_SET(8), 0);*/

void tx_callback(const struct device *dev, int error, void *user_data)
{
	LOG_INF("CAN TX callback, error: %d", error);
}

void rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
	LOG_INF("CAN RX callback, ID: 0x%x, DLC: %d", frame->id, frame->dlc);
	LOG_HEXDUMP_INF(frame->data, frame->dlc, "Data:");
}

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);
	int ret;

	can_stop(dev);

	/* Change to CAN_MODE_LOOPBACK for loopback testing */
	ret = can_set_mode(dev, CAN_MODE_NORMAL);
	if (ret) {
		LOG_ERR("Failed to set CAN mode: %d", ret);
		return ret;
	}

	ret = can_set_bitrate(dev, 800000);
	if (ret) {
		LOG_ERR("Failed to set CAN bitrate: %d", ret);
		return ret;
	}

	ret = can_start(dev);
	if (ret) {
		LOG_ERR("Failed to start CAN: %d", ret);
		return ret;
	}

	struct can_frame msg = {0};
	msg.id = 0x123;
	msg.dlc = 1;
	msg.data[0] = 0x42;

	ret = can_add_rx_filter(dev, rx_callback, NULL, &(struct can_filter){});
	if (ret) {
		LOG_ERR("Can't add RX filter: %d", ret);
		return ret;
	}

	for (;;) {
		LOG_INF("-----------------------------------");
		LOG_INF("bit errors: %d", can_stats_get_bit_errors(dev));
		LOG_INF("bit0 errors: %d", can_stats_get_bit0_errors(dev));
		LOG_INF("bit1 errors: %d", can_stats_get_bit1_errors(dev));
		LOG_INF("stuff errors: %d", can_stats_get_stuff_errors(dev));
		LOG_INF("CRC errors: %d", can_stats_get_crc_errors(dev));
		LOG_INF("form errors: %d", can_stats_get_form_errors(dev));
		LOG_INF("ack errors: %d", can_stats_get_ack_errors(dev));
		LOG_INF("RX overruns: %d", can_stats_get_rx_overruns(dev));

		ret = can_send(dev, &msg, K_MSEC(100), tx_callback, NULL);
		if (ret) {
			LOG_ERR("Failed to send CAN frame: %d", ret);
		} else {
			LOG_INF("CAN frame sent");
		}

		ctr_led_blink(ctr_led_mainboard, CTR_LED_CHANNEL_R, K_MSEC(30));
		k_sleep(K_MSEC(5000));
	}

	return 0;
}
