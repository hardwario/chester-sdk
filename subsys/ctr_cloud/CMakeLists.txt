#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources(ctr_cloud_packet.c)
zephyr_library_sources(ctr_cloud_msg.c)
zephyr_library_sources(ctr_cloud_transfer.c)
zephyr_library_sources(ctr_cloud_util.c)
zephyr_library_sources(ctr_cloud_process.c)
zephyr_library_sources(ctr_cloud_shell.c)
zephyr_library_sources(ctr_cloud.c)
