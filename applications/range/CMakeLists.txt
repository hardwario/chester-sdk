#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_ds18b20 ctr_lte ctr_mb7066_a ctr_mb7066_b ctr_s2 ctr_z
set(SHIELD ctr_ds18b20 ctr_lte ctr_mb7066_a)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(range)

target_sources(app PRIVATE src/app_backup.c)
target_sources(app PRIVATE src/app_cbor.c)
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
