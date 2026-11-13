
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_serial_piketronic_rppu)

# Pull in the pure frame codec straight from the serial application. It has no Zephyr
# or app dependencies, so it links into the host test without any mocks.
set(RPPU_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../applications/serial/src/drivers)

target_include_directories(app PRIVATE ${RPPU_DIR})
target_sources(app PRIVATE ${RPPU_DIR}/drv_piketronic_rppu_proto.c)
target_sources(app PRIVATE src/test_rppu_proto.c)
