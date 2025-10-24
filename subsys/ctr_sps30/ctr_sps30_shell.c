/*
 * Copyright (c) 2024 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/ctr_sps30.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ctr_sps30_shell, CONFIG_CTR_SPS30_LOG_LEVEL);

static int cmd_sps30_read(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	int repetitions = 1;

	if (argc == 2) {
		repetitions = strtoll(argv[1], NULL, 10);

		if (repetitions < 1 || repetitions > 3600) {
			shell_error(shell, "invalid range");
			return -EINVAL;
		}
	}

	for (int i = 0; i < repetitions; i++) {
		if (i) {
			k_sleep(K_SECONDS(1));
		}

		if (repetitions > 1) {
			shell_print(shell, "Repetition: %d/%d", i + 1, repetitions);
		}

		float mass_conc_pm_1_0;
		float mass_conc_pm_2_5;
		float mass_conc_pm_4_0;
		float mass_conc_pm_10_0;

		float num_conc_pm_0_5;
		float num_conc_pm_1_0;
		float num_conc_pm_2_5;
		float num_conc_pm_4_0;
		float num_conc_pm_10_0;
		ret = ctr_sps30_read(&mass_conc_pm_1_0, &mass_conc_pm_2_5, &mass_conc_pm_4_0,
				     &mass_conc_pm_10_0, &num_conc_pm_0_5, &num_conc_pm_1_0,
				     &num_conc_pm_2_5, &num_conc_pm_4_0, &num_conc_pm_10_0);
		if (ret) {
			LOG_ERR("Call `ctr_sps30_read` failed: %d", ret);
			shell_error(shell, "command failed");
			return ret;
		}

		shell_print(shell, "Mass Concentration PM1.0: %.2f ug/m3",
			    (double)mass_conc_pm_1_0);
		shell_print(shell, "Mass Concentration PM2.5: %.2f ug/m3",
			    (double)mass_conc_pm_2_5);
		shell_print(shell, "Mass Concentration PM4.0: %.2f ug/m3",
			    (double)mass_conc_pm_4_0);
		shell_print(shell, "Mass Concentration PM10.0: %.2f ug/m3",
			    (double)mass_conc_pm_10_0);

		shell_print(shell, "Number Concentration PM1.0: %.2f", (double)num_conc_pm_0_5);
		shell_print(shell, "Number Concentration PM1.0: %.2f", (double)num_conc_pm_1_0);
		shell_print(shell, "Number Concentration PM2.5: %.2f", (double)num_conc_pm_2_5);
		shell_print(shell, "Number Concentration PM4.0: %.2f", (double)num_conc_pm_4_0);
		shell_print(shell, "Number Concentration PM10.0: %.2f", (double)num_conc_pm_10_0);
	}

	return 0;
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_sps30,

	SHELL_CMD_ARG(read, NULL,
	              "Read sps30 sensor "
	              "w/ optional number of repetitions (format: [<1-3600>]).",
		      cmd_sps30_read, 1, 1),

	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(sps30, &sub_sps30, "Particulate Matter sensor commands.", print_help);

/* clang-format on */
