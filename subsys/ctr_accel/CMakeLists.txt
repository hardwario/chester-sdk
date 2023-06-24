#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_accel.c)
zephyr_library_sources_ifdef(CONFIG_CTR_ACCEL_SHELL ctr_accel_shell.c)
