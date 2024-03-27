
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

add_compile_definitions(CONFIG_CTR_CLOUD_LOG_LEVEL=4)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_packet.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_msg.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_transfer.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_util.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_process.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_shell.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud.c)

target_sources(app PRIVATE src/mock.c)
target_sources(app PRIVATE src/mock_ctr_info.c)
target_sources(app PRIVATE src/mock_ctr_lte_v2.c)
target_sources(app PRIVATE src/mock_ctr_rtc.c)

target_sources(app PRIVATE src/test_packet.c)
target_sources(app PRIVATE src/test_msg.c)

# target_sources(app PRIVATE src/test_cloud.c)
