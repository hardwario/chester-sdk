#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_lte ctr_x0_a ctr_x0_b ctr_z
set(SHIELD ctr_lte ctr_x0_a)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(counter)

target_sources_ifdef(CONFIG_APP_TAMPER app PRIVATE src/app_tamper.c)
target_sources(app PRIVATE src/app_backup.c)
target_sources(app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_counter.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_power.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_sensor.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/app_work.c)

target_sources(app PRIVATE src/main.c)
