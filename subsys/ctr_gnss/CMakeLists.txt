#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_gnss.c)
zephyr_library_sources_ifdef(CONFIG_CTR_GNSS_SHELL ctr_gnss_shell.c)
zephyr_library_sources_ifdef(CONFIG_SHIELD_CTR_GNSS minmea.c)
zephyr_library_sources_ifdef(CONFIG_SHIELD_CTR_GNSS ctr_gnss_m8.c)
