cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_x2 ctr_x3_a)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(weight_scale)

target_sources(app PRIVATE src/main.c)
