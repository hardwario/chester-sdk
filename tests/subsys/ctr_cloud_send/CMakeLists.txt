cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

# CTR_CLOUD depends on the CHESTER LTE shield, so its Kconfig symbols are not
# visible here and the sources under test are compiled by hand with these values.
add_compile_definitions(CONFIG_CTR_CLOUD_LOG_LEVEL=3)
add_compile_definitions(CONFIG_CTR_CLOUD_TRANSFER_BUF_SIZE=16384)
add_compile_definitions(CONFIG_CTR_CLOUD_ENCODE_ZCBOR_STATES=2)
if(NOT DEFINED TEST_SPOOL)
  set(TEST_SPOOL 1)
endif()
if(TEST_SPOOL)
  add_compile_definitions(CONFIG_CTR_CLOUD_SPOOL=1)
endif()
# Firmware update confirmed by the init work, see src/mock_boot.c
if(NOT DEFINED TEST_FW)
  set(TEST_FW 1)
endif()
add_compile_definitions(TEST_FW=${TEST_FW})

# The real ctr_cloud.c; only the transport (ctr_cloud_transfer_*) and the spool
# backend (ctr_cloud_spool_*) are mocked.
set(CLOUD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud)
set(MOCK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../ctr_cloud/src)

include_directories(${CLOUD_DIR})
include_directories(${MOCK_DIR})
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud.c)
# Only ctr_cloud.c: MCUboot itself is mocked (src/mock_boot.c)
set_source_files_properties(${CLOUD_DIR}/ctr_cloud.c PROPERTIES COMPILE_DEFINITIONS
                            CONFIG_MCUBOOT_IMG_MANAGER=1)
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_cbor.c)
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_msg_data.c)
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_msg.c)
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_process.c)
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_util.c)

target_sources(app PRIVATE ${MOCK_DIR}/mock.c)
target_sources(app PRIVATE ${MOCK_DIR}/mock_ctr_info.c)
target_sources(app PRIVATE ${MOCK_DIR}/mock_ctr_lte_v2.c)
target_sources(app PRIVATE ${MOCK_DIR}/mock_ctr_rtc.c)

target_sources(app PRIVATE src/common.c)
target_sources(app PRIVATE src/mock_boot.c)
target_sources(app PRIVATE src/mock_spool.c)
target_sources(app PRIVATE src/mock_transfer.c)

target_sources(app PRIVATE src/test_send_data.c)
target_sources(app PRIVATE src/test_send_data_cb.c)
target_sources(app PRIVATE src/test_firmware.c)
target_sources(app PRIVATE src/test_init.c)
