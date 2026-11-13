/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef CHESTER_INCLUDE_DRV_SC16IS7XX_H_
#define CHESTER_INCLUDE_DRV_SC16IS7XX_H_

/* Zephyr includes */
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Driver-specific commands for the NXP SC16IS7xx I2C UART bridge.
 *
 * Pass to uart_drv_cmd(). Requires CONFIG_UART_DRV_CMD=y.
 */

/**
 * @brief Assert (p != 0) or deassert (p == 0) the LCR break-control bit.
 *
 * While asserted, the bridge forces its TX line to logic 0 and holds it there.
 * Required by peripherals that expect the host to park their RX input low while
 * idle - notably the Piketronic RPP-U radon probe, which enters power-save when
 * its RX_in is 0 and only re-arms on a 0 -> 1 transition held for 2 s.
 */
#define SC16IS7XX_DRV_CMD_SET_BREAK 0x01

/**
 * @brief Convenience wrapper around uart_drv_cmd() for the break bit.
 *
 * @param dev SC16IS7xx UART device
 * @param set true to force TX low, false to resume normal idle-high operation
 *
 * @retval 0 on success
 * @retval -ENOTSUP if the driver was built without CONFIG_UART_DRV_CMD
 */
static inline int drv_sc16is7xx_set_break(const struct device *dev, bool set)
{
	return uart_drv_cmd(dev, SC16IS7XX_DRV_CMD_SET_BREAK, set ? 1 : 0);
}

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_DRV_SC16IS7XX_H_ */
