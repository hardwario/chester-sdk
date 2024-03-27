
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

add_compile_definitions(CONFIG_CTR_LTE_V2_LOG_LEVEL=4)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_tok.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_parse.c)

target_sources(app PRIVATE src/test_parse.c)
