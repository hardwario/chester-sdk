
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

add_compile_definitions(CONFIG_CTR_RFMUX_LOG_LEVEL=4)
add_compile_definitions(CONFIG_CTR_LTE_LINK_LOG_LEVEL=3)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2)

target_sources(app PRIVATE src/mock_ctr_rfmux.c)
target_sources(app PRIVATE src/mock_ctr_lte_link.c)
target_sources(app PRIVATE src/mock_ctr_rtc.c)
target_sources(app PRIVATE src/mock_ctr_info.c)

target_sources_ifdef(CONFIG_TEST_FEATURE_PARSING app PRIVATE src/test_parse.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_STACK app PRIVATE src/test_stack.c)
