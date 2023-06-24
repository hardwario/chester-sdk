/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

/* CHESTER includes */
#include <chester/drivers/ctr_s1.h>

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

LOG_MODULE_DECLARE(ctr_s1, CONFIG_CTR_S1_LOG_LEVEL);

static const struct device *m_dev = DEVICE_DT_GET(DT_NODELABEL(ctr_s1));

static int cmd_calibrate_co2(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	int co2_target_ppm = strtol(argv[1], NULL, 10);

	if (co2_target_ppm < 0 || co2_target_ppm > 5000) {
		shell_error(shell, "invalid range");
		return -EINVAL;
	}

	if (!device_is_ready(m_dev)) {
		LOG_ERR("Device not ready");
		return -ENODEV;
	}

	ret = ctr_s1_calib_tgt_co2_conc(m_dev, co2_target_ppm);
	if (ret) {
		LOG_ERR("Call `ctr_s1_calib_tgt_co2_conc` failed: %d", ret);
		return ret;
	}

	return 0;
}

/* clang-format off */

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_iaq,

	SHELL_CMD_ARG(calibrate-co2, NULL, "Calibrate CO2 sensor to target concentration in ppm (format: <0-5000>).", cmd_calibrate_co2, 2, 0),

        SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(iaq, &sub_iaq, "IAQ sensor commands.", NULL);

/* clang-format on */
