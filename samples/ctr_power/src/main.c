/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_power.h>
#include <chester/drivers/ctr_z.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static void power_event_handler(const struct ctr_power_event *event, void *user_data)
{
	LOG_INF("Report requested: line %d, timestamp %lld", (int)event->line, event->timestamp);
}

#if DT_NODE_HAS_STATUS(DT_NODELABEL(ctr_z), okay)

/*
 * Registered alongside the handler ctr_power installs on the same device, which is what
 * the multi-handler support in the drivers exists for.
 */
static void ctr_z_event_handler(const struct device *dev, enum ctr_z_event event, void *user_data)
{
	LOG_INF("CHESTER-Z event: %d", (int)event);
}

static int init_ctr_z_buttons(void)
{
	int ret;

	static const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ctr_z));

	if (!device_is_ready(dev)) {
		LOG_INF("CHESTER-Z not detected");
		return 0;
	}

	ret = ctr_z_set_handler(dev, ctr_z_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_z_set_handler` failed: %d", ret);
		return ret;
	}

	return 0;
}

#else

static int init_ctr_z_buttons(void)
{
	return 0;
}

#endif

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = ctr_power_init(power_event_handler, NULL);
	if (ret) {
		LOG_ERR("Call `ctr_power_init` failed: %d", ret);
		k_oops();
	}

	ret = init_ctr_z_buttons();
	if (ret) {
		LOG_ERR("Call `init_ctr_z_buttons` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");
		k_sleep(K_SECONDS(10));
	}

	return 0;
}
