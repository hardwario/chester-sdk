cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_machine_probe)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ctr_machine_probe)

target_sources(app PRIVATE src/main.c)
