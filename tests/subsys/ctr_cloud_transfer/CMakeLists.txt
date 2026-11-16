cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

# CTR_CLOUD depends on the CHESTER LTE shield, so its Kconfig symbols are not
# visible here and the sources under test are compiled by hand with these values.
add_compile_definitions(CONFIG_CTR_CLOUD_LOG_LEVEL=3)

# The real ctr_cloud_transfer.c over a scripted ctr_lte_v2_send_recv()
set(CLOUD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../subsys/ctr_cloud)

include_directories(${CLOUD_DIR})
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_packet.c)
target_sources(app PRIVATE ${CLOUD_DIR}/ctr_cloud_transfer.c)

target_sources(app PRIVATE src/mock_ctr_lte_v2.c)
target_sources(app PRIVATE src/test_transfer.c)
