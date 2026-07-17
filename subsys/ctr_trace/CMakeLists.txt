#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_CTR_TRACE ctr_trace.c)
