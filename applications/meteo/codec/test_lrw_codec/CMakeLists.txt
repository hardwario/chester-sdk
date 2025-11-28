
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lrw_codec)

#add_compile_definitions(CONFIG_CTR_CLOUD_LOG_LEVEL=4)

#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../application/src)
#target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../application/src/app_thrs.c)

add_compile_definitions(FEATURE_SUBSYSTEM_LRW)

add_compile_definitions(CONFIG_CTR_BATT)
add_compile_definitions(CONFIG_CTR_ACCEL)
add_compile_definitions(CONFIG_CTR_THERM)
add_compile_definitions(FEATURE_HARDWARE_CHESTER_S2)
add_compile_definitions(FEATURE_SUBSYSTEM_DS18B20)
add_compile_definitions(FEATURE_SUBSYSTEM_BLE_TAG)
#add_compile_definitions(FEATURE_HARDWARE_CHESTER_X0_A)
add_compile_definitions(FEATURE_HARDWARE_CHESTER_METEO_A)
add_compile_definitions(FEATURE_HARDWARE_CHESTER_BAROMETER_TAG)
add_compile_definitions(FEATURE_SUBSYSTEM_SOIL_SENSOR)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_sources(app PRIVATE encode.c)
target_sources(app PRIVATE ../../src/app_lrw.c)
target_sources(app PRIVATE ../../src/app_data.c)


