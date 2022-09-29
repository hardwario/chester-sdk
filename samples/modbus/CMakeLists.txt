cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_x2_a)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(modbus)

target_sources(app PRIVATE src/main.c)
