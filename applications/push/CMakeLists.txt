cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(push)

target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/main.c)