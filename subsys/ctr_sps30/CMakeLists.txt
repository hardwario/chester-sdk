#
# Copyright (c) 2024 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_sps30.c)
zephyr_library_sources_ifdef(CONFIG_CTR_SPS30_SHELL ctr_sps30_shell.c)
