#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CTR_DS18B20_SHELL ctr_ds18b20_shell.c)
zephyr_library_sources(ctr_ds18b20.c)
