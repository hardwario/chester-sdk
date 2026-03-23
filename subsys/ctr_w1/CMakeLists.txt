#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_w1.c)

zephyr_library_sources_ifdef(CONFIG_CTR_W1_SHELL ctr_w1_shell.c)
