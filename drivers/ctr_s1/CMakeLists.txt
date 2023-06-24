#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_s1.c)
zephyr_library_sources_ifdef(CONFIG_CTR_S1_SHELL ctr_s1_shell.c)

