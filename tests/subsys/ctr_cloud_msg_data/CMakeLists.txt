cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

add_compile_definitions(CONFIG_CTR_CLOUD_LOG_LEVEL=4)
# CTR_CLOUD depends on the CHESTER LTE shield, so its Kconfig symbols are not
# visible here and the sources under test are compiled by hand with this value.
if(NOT DEFINED TEST_ZCBOR_STATES)
  set(TEST_ZCBOR_STATES 2)
endif()
add_compile_definitions(CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES=${TEST_ZCBOR_STATES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_msg_data.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud/ctr_cloud_cbor.c)

target_sources(app PRIVATE src/test_pack_data.c)
