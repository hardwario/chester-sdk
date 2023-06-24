#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_lrw ctr_x0_a ctr_x0_b ctr_s2 ctr_z
set(SHIELD ctr_lrw ctr_x0_a ctr_x0_b ctr_z)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(input)

target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_shell.c)

target_sources(app PRIVATE src/main.c)
