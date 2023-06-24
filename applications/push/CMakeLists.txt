#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_lrw ctr_lte ctr_z
set(SHIELD ctr_lrw ctr_lte ctr_z)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(push)

target_sources(app PRIVATE src/app_backup.c)
target_sources_ifdef(CONFIG_SHIELD_CTR_LTE app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_power.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_sensor.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/app_work.c)

target_sources(app PRIVATE src/main.c)
