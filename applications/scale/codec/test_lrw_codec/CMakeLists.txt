
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lrw_codec)

add_compile_definitions(FEATURE_SUBSYSTEM_LRW)

add_compile_definitions(CONFIG_CTR_BATT)
add_compile_definitions(CONFIG_CTR_ACCEL)
add_compile_definitions(CONFIG_CTR_THERM)
add_compile_definitions(FEATURE_HARDWARE_CHESTER_X3_A)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_sources(app PRIVATE encode.c)
target_sources(app PRIVATE ../../src/app_lrw.c)
target_sources(app PRIVATE ../../src/app_data.c)

