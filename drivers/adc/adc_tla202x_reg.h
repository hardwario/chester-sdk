/*
 * Copyright (c) 2023 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef ADC_TLA202X_REG_H_
#define ADC_TLA202X_REG_H_

#define REG_DATA     0x00
#define REG_DATA_POS 4

#define REG_CONFIG	    0x01
#define REG_CONFIG_DEFAULT  0x0003
#define REG_CONFIG_DR_POS   5
#define REG_CONFIG_MODE_POS 8
#define REG_CONFIG_PGA_POS  9
#define REG_CONFIG_MUX_POS  12
#define REG_CONFIG_OS_POS   15
#define REG_CONFIG_OS_MSK   BIT(15)

#define REG_CONFIG_MODE_DEFAULT 1
#define REG_CONFIG_PGA_DEFAULT	2
#define REG_CONFIG_MUX_DEFAULT	0

#endif /* !ADC_TLA202X_REG_H_ */