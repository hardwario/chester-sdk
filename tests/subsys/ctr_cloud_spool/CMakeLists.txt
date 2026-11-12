cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

add_compile_definitions(CONFIG_CTR_CLOUD_LOG_LEVEL=4)
add_compile_definitions(CONFIG_CTR_CLOUD_SPOOL=1)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_spool_littlefs.c)

# gmtime_r() is POSIX - picolibc on native_sim declares it only on request
# (CHESTER boards get it from newlib)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_spool_littlefs.c
                            PROPERTIES COMPILE_DEFINITIONS _POSIX_C_SOURCE=200809L)

target_sources(app PRIVATE src/test_spool.c)
