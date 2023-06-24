#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_lte ctr_x3_a ctr_x3_b ctr_z people_counter
set(SHIELD ctr_lte ctr_x3_a)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(scale)

target_sources(app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_loop.c)
target_sources(app PRIVATE src/app_measure.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_shell.c)

target_sources(app PRIVATE src/main.c)
