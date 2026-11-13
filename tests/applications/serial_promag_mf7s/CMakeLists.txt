
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_serial_promag_mf7s)

set(DRV_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../applications/serial/src/drivers)

target_include_directories(app PRIVATE ${DRV_DIR})
target_sources(app PRIVATE ${DRV_DIR}/drv_promag_mf7s_proto.c)
target_sources(app PRIVATE src/test_promag_proto.c)
