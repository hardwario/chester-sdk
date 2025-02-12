/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_led.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	const struct device *w5500 = DEVICE_DT_GET(DT_NODELABEL(ctr_x14_w5500));

	if (!device_is_ready(w5500)) {
		LOG_ERR("W5500 device is not ready");
		ctr_led_set(CTR_LED_CHANNEL_R, true);
		return -ENODEV;
	}

	struct net_if *iface = net_if_get_default();
	while (!net_if_is_up(iface)) {
		LOG_INF("Waiting for interface to be up");
		k_sleep(K_MSEC(1000));
	}

	struct in_addr addr = {.s4_addr = {192, 168, 255, 1}};
	net_if_ipv4_addr_add(iface, &addr, NET_ADDR_MANUAL, 0);
	struct in_addr gw = {.s4_addr = {192, 168, 255, 1}};
	net_if_ipv4_set_gw(iface, &gw);
	struct in_addr mask = {.s4_addr = {255, 255, 255, 0}};
	net_if_ipv4_set_netmask(iface, &mask);

	for (;;) {
		k_sleep(K_MSEC(5000));
		LOG_INF("Alive");
	}

	return 0;
}
