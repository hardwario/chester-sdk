
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
target_sources_ifdef(CONFIG_TEST_FEATURE_PARSING app PRIVATE src/test_state.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_PARSING app PRIVATE src/test_str.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_PARSING app PRIVATE src/test_fmt_duration.c)

if(CONFIG_TEST_FEATURE_PARSING AND CONFIG_CTR_LTE_V2_SCAN)
  target_sources(app PRIVATE src/test_parse_scan.c)
endif()

target_sources_ifdef(CONFIG_TEST_FEATURE_STACK app PRIVATE src/test_stack.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_TXN app PRIVATE src/test_txn.c)
target_sources_ifdef(CONFIG_TEST_FEATURE_SCAN app PRIVATE src/test_scan.c)

if(CONFIG_TEST_FEATURE_STACK OR CONFIG_TEST_FEATURE_TXN OR CONFIG_TEST_FEATURE_SCAN)
  target_sources(app PRIVATE src/stack.c)
endif()

# The 'lte' shell commands, without CONFIG_CTR_SHELL (it selects the RTT backend)
if(CONFIG_TEST_FEATURE_TXN OR CONFIG_TEST_FEATURE_SCAN)
  target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_shell.c)
endif()
