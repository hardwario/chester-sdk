#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_x12_a)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(chester_x12_loop)

target_sources(app PRIVATE src/main.c)
