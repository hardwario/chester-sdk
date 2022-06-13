cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_lte ctr_x3_a ctr_x3_b
set(SHIELD ctr_lte ctr_x3_a)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(scale)

target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/main.c)
