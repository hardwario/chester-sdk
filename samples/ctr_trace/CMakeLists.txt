#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Apply the ctr-trace snippet by default (reserves the retained-RAM region and
# enables the subsystem). Override on the command line with e.g. `-S ""` if not
# wanted.
set(SNIPPET ctr-trace CACHE STRING "")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ctr_trace)

target_sources(app PRIVATE src/main.c)
