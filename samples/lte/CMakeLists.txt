cmake_minimum_required(VERSION 3.20.0)
set(BOARD chester_nrf52840)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lte)

target_sources(app PRIVATE src/main.c)
