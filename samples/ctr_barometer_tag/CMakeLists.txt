cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_barometer_tag)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ctr_barometer_tag)

target_sources(app PRIVATE src/main.c)
