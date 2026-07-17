/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * ctr_trace sample.
 *
 * The subsystem initializes itself (via the `ctr-trace` snippet) and captures
 * this firmware's logs into a retained-RAM ring buffer. Use the `demo` shell
 * commands to provoke a fault; after the device reboots, inspect the captured
 * state:
 *
 *   trace reset show   # boot/reset records (cause + timestamp)
 *   trace fault show   # logs captured just before the last fault
 *   trace log show     # the current boot's log ring buffer
 *
 * A hardware watchdog is armed the same way real CHESTER applications do it, so
 * `demo hang` reliably provokes a watchdog reset (captured as a fault) instead
 * of hanging forever.
 */

/* CHESTER includes */
#include <chester/ctr_wdog.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static struct ctr_wdog_channel m_wdog_channel;

static int cmd_demo_crash(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "triggering kernel panic");
	LOG_ERR("About to panic on purpose");
	k_panic();

	return 0;
}

static int cmd_demo_busfault(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "triggering bus fault (null dereference)");
	LOG_ERR("About to dereference NULL on purpose");

	volatile uint32_t *p = NULL;
	*p = 0xdeadbeef;

	return 0;
}

static int cmd_demo_hang(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "locking interrupts and spinning (expect watchdog reset)");
	LOG_ERR("About to hang on purpose");

	(void)irq_lock();
	for (;;) {
	}

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_demo,

	SHELL_CMD_ARG(crash, NULL, "Trigger a kernel panic.", cmd_demo_crash, 1, 0),
	SHELL_CMD_ARG(busfault, NULL, "Trigger a bus fault (NULL dereference).",
		      cmd_demo_busfault, 1, 0),
	SHELL_CMD_ARG(hang, NULL, "Lock interrupts and spin (watchdog reset).", cmd_demo_hang, 1, 0),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(demo, &sub_demo, "ctr_trace demo fault commands.", NULL);

/* clang-format on */

int main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	/* Arm the hardware watchdog like a real application (120 s timeout, fed
	 * from the heartbeat loop). This is what turns `demo hang` into a
	 * watchdog reset. */
	ret = ctr_wdog_set_timeout(10000);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_set_timeout` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_install(&m_wdog_channel);
	if (ret) {
		LOG_ERR("Call `ctr_wdog_install` failed: %d", ret);
		k_oops();
	}

	ret = ctr_wdog_start();
	if (ret) {
		LOG_ERR("Call `ctr_wdog_start` failed: %d", ret);
		k_oops();
	}

	int counter = 0;

	for (;;) {
		LOG_INF("Alive, counter %d", counter++);

		ret = ctr_wdog_feed(&m_wdog_channel);
		if (ret) {
			LOG_ERR("Call `ctr_wdog_feed` failed: %d", ret);
			k_oops();
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
