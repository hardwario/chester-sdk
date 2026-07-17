/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 *
 * Black-box tests of the `trace` shell command interface, driven through the
 * dummy shell backend. Exercises the real subsystem state initialized by
 * ctr_trace's SYS_INIT (which recorded this boot into the chosen retained_mem
 * device).
 */

/* Compile the subsystem source in directly (it is not linked via
 * CONFIG_CTR_TRACE): this brings in its SYS_INIT, log backend and the `trace`
 * shell command set that these black-box tests drive. */
#include "ctr_trace.c"

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

#include <string.h>

static int run_cmd(const char *cmd, const char **out)
{
	const struct shell *sh = shell_backend_dummy_get_ptr();

	shell_backend_dummy_clear_output(sh);

	int ret = shell_execute_cmd(sh, cmd);

	size_t size;
	*out = shell_backend_dummy_get_output(sh, &size);

	return ret;
}

static void *suite_setup(void)
{
	/* Let the shell backend finish initializing before driving it. */
	k_sleep(K_MSEC(500));
	return NULL;
}

ZTEST_SUITE(ctr_trace_shell, NULL, suite_setup, NULL, NULL, NULL);

/* Tests are prefixed to force run order: the read-only "show" checks run before
 * the destructive "clear" tests (ztest executes tests alphabetically). */
ZTEST(ctr_trace_shell, test_1_reset_show_has_boot_record)
{
	const char *out;
	int ret = run_cmd("trace reset show", &out);

	zassert_ok(ret, "command returned %d, output: %s", ret, out);
	/* SYS_INIT recorded the first boot. */
	zassert_true(strstr(out, "boot 1") != NULL, "expected a boot record, output: %s", out);
}

ZTEST(ctr_trace_shell, test_2_fault_show_empty_on_clean_boot)
{
	const char *out;
	int ret = run_cmd("trace fault show", &out);

	zassert_ok(ret, "command returned %d, output: %s", ret, out);
	zassert_true(strstr(out, "no fault snapshot") != NULL, "output: %s", out);
}

ZTEST(ctr_trace_shell, test_3_log_show_and_clear)
{
	const char *out;

	/* The primary ring already holds boot-time logs; just confirm show works. */
	int ret = run_cmd("trace log show", &out);

	zassert_ok(ret, "command returned %d, output: %s", ret, out);

	/* Clear and confirm it empties. */
	ret = run_cmd("trace log clear", &out);
	zassert_ok(ret, "clear returned %d, output: %s", ret, out);
	zassert_true(strstr(out, "command succeeded") != NULL, "output: %s", out);

	ret = run_cmd("trace log show", &out);
	zassert_ok(ret, "command returned %d, output: %s", ret, out);
	zassert_true(strstr(out, "<empty>") != NULL, "log must be empty after clear, output: %s",
		     out);
}

ZTEST(ctr_trace_shell, test_4_reset_clear)
{
	const char *out;

	int ret = run_cmd("trace reset clear", &out);

	zassert_ok(ret, "clear returned %d, output: %s", ret, out);

	ret = run_cmd("trace reset show", &out);
	zassert_ok(ret, "command returned %d, output: %s", ret, out);
	zassert_true(strstr(out, "<empty>") != NULL,
		     "records must be empty after clear, output: %s", out);
}
