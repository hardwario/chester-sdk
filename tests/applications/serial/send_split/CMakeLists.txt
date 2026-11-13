cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test)

# The module under test is an application source without Zephyr device or
# configuration dependencies; compile it directly from the application tree.
set(SERIAL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../../../applications/serial/src)

include_directories(${SERIAL_SRC})

target_sources(app PRIVATE ${SERIAL_SRC}/app_send_split.c)
target_sources(app PRIVATE src/test_split.c)
