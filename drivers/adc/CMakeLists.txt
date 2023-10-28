#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ADC_ADS122C04 adc_ads122c04.c)
zephyr_library_sources_ifdef(CONFIG_ADC_TLA2024 adc_tla2024.c)
