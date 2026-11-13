#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(ENV{FW_BUNDLE} "com.hardwario.chester.app.serial.rs232")
set(ENV{FW_NAME} "CHESTER Serial RS-232")

# Derive firmware version from git (see scripts/version.cmake); the fallback is
# used when git is unavailable at build time.
set(FW_VERSION_FALLBACK "v3.5.3")
include(${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/version.cmake)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(chester-serial-rs232)

include(${ZEPHYR_CHESTER_MODULE_DIR}/scripts/app_codec.cmake)

target_sources(app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_device.c)
target_sources(app PRIVATE src/app_driver.c)
target_sources(app PRIVATE src/app_driver_store.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_lrw.c)
target_sources(app PRIVATE src/app_modbus.c)
target_sources(app PRIVATE src/app_modbus_addr.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_send_split.c)
target_sources(app PRIVATE src/app_sensor.c)
target_sources(app PRIVATE src/app_serial.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/app_work.c)
target_sources(app PRIVATE src/drivers/drv_cubic_6303.c)
target_sources(app PRIVATE src/drivers/drv_em1xx.c)
target_sources(app PRIVATE src/drivers/drv_em5xx.c)
target_sources(app PRIVATE src/drivers/drv_flowt_ft201.c)
target_sources(app PRIVATE src/drivers/drv_iem3000.c)
target_sources(app PRIVATE src/drivers/drv_microsens_180hs.c)
target_sources(app PRIVATE src/drivers/drv_microsens_180hs_proto.c)
target_sources(app PRIVATE src/drivers/drv_or_we_504.c)
target_sources(app PRIVATE src/drivers/drv_or_we_516.c)
target_sources(app PRIVATE src/drivers/drv_piketronic_rpp.c)
target_sources(app PRIVATE src/drivers/drv_piketronic_rppu.c)
target_sources(app PRIVATE src/drivers/drv_piketronic_rppu_proto.c)
target_sources(app PRIVATE src/drivers/drv_promag_mf7s.c)
target_sources(app PRIVATE src/drivers/drv_promag_mf7s_proto.c)
target_sources(app PRIVATE src/drivers/drv_sensecap.c)
target_sources(app PRIVATE src/drivers/drv_solax_g3.c)
target_sources(app PRIVATE src/main.c)

target_precompile_headers(app PRIVATE src/feature.h)

# ### Preserved code "cmake" (begin)
# Access to the Zephyr Modbus internal API (modbus_internal.h) for the RTU
# frame-timeout workaround in app_modbus.c (SC16IS7xx I2C-UART bridge RX batching)
# and for the frames of the OR-WE-504 address change.
target_include_directories(app PRIVATE $ENV{ZEPHYR_BASE}/subsys/modbus)
# ^^^ Preserved code "cmake" (end)
