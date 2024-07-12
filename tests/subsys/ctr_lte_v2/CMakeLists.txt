
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

add_compile_definitions(CONFIG_CTR_RFMUX_LOG_LEVEL=4)
add_compile_definitions(CONFIG_CTR_LTE_LINK_LOG_LEVEL=3)
add_compile_definitions(CONFIG_CTR_LTE_V2_LOG_LEVEL=4)

add_compile_definitions(CONFIG_CTR_LTE_V2_INIT_PRIORITY=99)
add_compile_definitions(CONFIG_CTR_LTE_V2_THREAD_STACK_SIZE=4096)
add_compile_definitions(CONFIG_CTR_LTE_V2_THREAD_PRIORITY=10)
add_compile_definitions(CONFIG_CTR_LTE_V2_GNSS=n)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_config.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_shell.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_flow.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_parse.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_state.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_talk.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2_tok.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_lte_v2/ctr_lte_v2.c)

target_sources(app PRIVATE src/mock_ctr_rfmux.c)
target_sources(app PRIVATE src/mock_ctr_lte_link.c)
target_sources(app PRIVATE src/mock_ctr_rtc.c)
target_sources(app PRIVATE src/mock_ctr_info.c)

target_sources(app PRIVATE src/test_parse.c)
target_sources(app PRIVATE src/test_stack.c)
