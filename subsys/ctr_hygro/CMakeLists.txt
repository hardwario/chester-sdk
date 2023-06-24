#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_hygro.c)
zephyr_library_sources_ifdef(CONFIG_CTR_HYGRO_SHELL ctr_hygro_shell.c)
