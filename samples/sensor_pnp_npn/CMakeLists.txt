#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_x0_a)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_pnp_npn)

target_sources(app PRIVATE src/main.c)
