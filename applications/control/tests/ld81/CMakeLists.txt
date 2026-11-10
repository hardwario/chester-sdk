cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ld81_sim)

# Only what drv_ld81.c needs; no hardware, no X0, no ADC.
add_compile_definitions(FEATURE_HARDWARE_CHESTER_X0_A)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_sources(app PRIVATE main.c)
target_sources(app PRIVATE ../../src/drivers/drv_ld81.c)
