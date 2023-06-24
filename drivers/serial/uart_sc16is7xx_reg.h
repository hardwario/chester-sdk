/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef DRIVERS_SERIAL_SC16IS7XX_REG_H_
#define DRIVERS_SERIAL_SC16IS7XX_REG_H_

/* Zephyr includes */
#include <zephyr/kernel.h>

#define SC16IS7XX_REG_SHIFT 3
#define SC16IS7XX_LCR_MAGIC 0xbf

/* General register set */
#define SC16IS7XX_REG_RHR	0x00
#define SC16IS7XX_REG_THR	0x00
#define SC16IS7XX_REG_IER	0x01
#define SC16IS7XX_REG_IIR	0x02 /* Read only, overlaps with FCR */
#define SC16IS7XX_REG_FCR	0x02 /* Write only, overlaps with IIR */
#define SC16IS7XX_REG_LCR	0x03
#define SC16IS7XX_REG_MCR	0x04
#define SC16IS7XX_REG_LSR	0x05
#define SC16IS7XX_REG_MSR	0x06
#define SC16IS7XX_REG_TCR	0x06 /* Accessible only if MCR[2] == 1 && EFR[4] == 1 */
#define SC16IS7XX_REG_SPR	0x07
#define SC16IS7XX_REG_TLR	0x07 /* Accessible only if MCR[2] == 1 && EFR[4] == 1 */
#define SC16IS7XX_REG_TXLVL	0x08
#define SC16IS7XX_REG_RXLVL	0x09
#define SC16IS7XX_REG_IODIR	0x0a
#define SC16IS7XX_REG_IOSTATE	0x0b
#define SC16IS7XX_REG_IOINTENA	0x0c
#define SC16IS7XX_REG_IOCONTROL 0x0e
#define SC16IS7XX_REG_EFCR	0x0f

/* Special register set (only accessible if LCR[7] == 1 && LCR != 0xbf) */
#define SC16IS7XX_REG_DLL 0x00
#define SC16IS7XX_REG_DLH 0x01

/* Enhanced register set (only accessible if LCR == 0xbf) */
#define SC16IS7XX_REG_EFR 0x02

/* IER register bits */
#define SC16IS7XX_IER_RHRI_BIT	     0 /* Receive Holding Register interrupt */
#define SC16IS7XX_IER_THRI_BIT	     1 /* Transmit Holding Register interrupt */
#define SC16IS7XX_IER_RLSI_BIT	     2 /* Receive Line Status interrupt */
#define SC16IS7XX_IER_SLEEP_MODE_BIT 4

/* IIR register bits */
#define SC16IS7XX_IIR_INTERRUPT_STATUS_BIT 0
#define SC16IS7XX_IIR_INTERRUPT_MASK	   (BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(1))
#define SC16IS7XX_IIR_RLSE_MASK		   (BIT(2) | BIT(1)) /* Receiver Line Status error */
#define SC16IS7XX_IIR_RTOI_MASK		   (BIT(3) | BIT(2)) /* Receiver time-out interrupt */
#define SC16IS7XX_IIR_RHRI_MASK		   BIT(2)	     /* RHR interrupt */
#define SC16IS7XX_IIR_THRI_MASK		   BIT(1)	     /* THR interrupt */

/* FCR register bits */
#define SC16IS7XX_FCR_FIFO_ENABLE_BIT	 0
#define SC16IS7XX_FCR_RESET_RX_FIFO_BIT	 1
#define SC16IS7XX_FCR_RESET_TX_FIFO_BIT	 2
#define SC16IS7XX_FCR_TX_TRIGGER_LSB_BIT 4
#define SC16IS7XX_FCR_TX_TRIGGER_MSB_BIT 5
#define SC16IS7XX_FCR_RX_TRIGGER_LSB_BIT 6
#define SC16IS7XX_FCR_RX_TRIGGER_MSB_BIT 7

/* LCR register bits */
#define SC16IS7XX_LCR_WORDLEN_LSB_BIT	       0
#define SC16IS7XX_LCR_WORDLEN_MSB_BIT	       1
#define SC16IS7XX_LCR_STOPLEN_BIT	       2
#define SC16IS7XX_LCR_PARITY_ENABLE_BIT	       3
#define SC16IS7XX_LCR_PARITY_TYPE_BIT	       4 /* 0: Odd, 1: Even*/
#define SC16IS7XX_LCR_FORCE_PARITY_BIT	       5
#define SC16IS7XX_LCR_DIVISOR_LATCH_ENABLE_BIT 7

/* MCR register bits */
#define SC16IS7XX_MCR_CLOCK_DIVISOR_BIT 7

/* EFCR register bits */
#define SC16IS7XX_EFCR_RTS_CONTROL_BIT 4
#define SC16IS7XX_EFCR_RTS_INVERT_BIT  5

/* IOControl register bits */
#define SC16IS7XX_IOCONTROL_IOLATCH_BIT	 0
#define SC16IS7XX_IOCONTROL_GPIO_7_4_BIT 1
#define SC16IS7XX_IOCONTROL_SRESET_BIT	 3

/* EFR register bits */
#define SC16IS7XX_EFR_EFE_BIT 4

#endif /* DRIVERS_SERIAL_SC16IS7XX_REG_H_ */
