#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_tc.c)
zephyr_library_sources_ifdef(CONFIG_CTR_TC_SHELL ctr_tc_shell.c)
