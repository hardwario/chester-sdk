cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_lte_v2)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ctr_cloud)

target_sources(app PRIVATE src/main.c)
