cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

# The modules under test are application sources without Zephyr device or
# configuration dependencies; compile them directly from the application tree.
set(SERIAL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../../applications/serial/src)

include_directories(${SERIAL_SRC})

target_sources(app PRIVATE ${SERIAL_SRC}/app_driver_store.c)
target_sources(app PRIVATE ${SERIAL_SRC}/app_modbus_addr.c)
target_sources(app PRIVATE src/test_plan.c)
target_sources(app PRIVATE src/test_records.c)
target_sources(app PRIVATE src/test_items.c)
target_sources(app PRIVATE src/test_modbus_decode.c)
target_sources(app PRIVATE src/test_modbus_addr.c)
