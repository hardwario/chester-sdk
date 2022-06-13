cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_s2)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hygro)

target_sources(app PRIVATE src/main.c)
