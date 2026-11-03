#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Switch to ctr_x4_a, ctr_x4_b or ctr_x10 to exercise the other power sources.
set(SHIELD ctr_z ctr_x4_a)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ctr_power)

target_sources(app PRIVATE src/main.c)
