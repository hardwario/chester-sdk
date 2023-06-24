#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

if(DEFINED ENV{FW_NAME})
add_definitions(-DFW_NAME=$ENV{FW_NAME})
endif()

if(DEFINED ENV{FW_VERSION})
add_definitions(-DFW_VERSION=$ENV{FW_VERSION})
endif()

zephyr_library_sources(ctr_info.c)
zephyr_library_sources_ifdef(CONFIG_CTR_INFO_BLE ctr_info_ble.c)
zephyr_library_sources_ifdef(CONFIG_CTR_INFO_SHELL ctr_info_shell.c)
