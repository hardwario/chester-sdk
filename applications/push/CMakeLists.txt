cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_lrw ctr_z)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(push)

target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_loop.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/main.c)
