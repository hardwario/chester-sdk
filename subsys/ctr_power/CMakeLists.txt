#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_power.c)
zephyr_library_sources(ctr_power_config.c)
zephyr_library_sources_ifdef(CONFIG_CTR_POWER_CBOR ctr_power_cbor.c)
zephyr_library_sources_ifdef(CONFIG_CTR_POWER_LRW ctr_power_lrw.c)
