cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_mb7066_a)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mb7066)

target_sources(app PRIVATE src/main.c)
