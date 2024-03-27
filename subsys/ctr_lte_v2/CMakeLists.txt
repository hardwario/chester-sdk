#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_lte_v2_config.c)
zephyr_library_sources(ctr_lte_v2_shell.c)
zephyr_library_sources(ctr_lte_v2_flow.c)
zephyr_library_sources(ctr_lte_v2_parse.c)
zephyr_library_sources(ctr_lte_v2_state.c)
zephyr_library_sources(ctr_lte_v2_talk.c)
zephyr_library_sources(ctr_lte_v2_tok.c)
zephyr_library_sources(ctr_lte_v2.c)
