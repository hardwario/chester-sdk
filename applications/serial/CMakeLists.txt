#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(ENV{FW_BUNDLE} "com.hardwario.chester.app.serial")
set(ENV{FW_VERSION} "v3.5.3")
set(ENV{FW_NAME} "CHESTER Serial")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(chester-serial)

add_custom_command(
    COMMAND west gen-codec -d ../codec/cbor-decoder.yaml -e ../codec/cbor-encoder.yaml -o ../src/app_codec.h
    OUTPUT ../src/app_codec.h
    DEPENDS ../codec/cbor-decoder.yaml
)

target_sources(app PRIVATE src/app_backup.c)
target_sources(app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_codec.h)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_device.c)
target_sources(app PRIVATE src/app_driver.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_lrw.c)
target_sources(app PRIVATE src/app_modbus.c)
target_sources(app PRIVATE src/app_power.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_sensor.c)
target_sources(app PRIVATE src/app_serial.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/app_work.c)
target_sources(app PRIVATE src/drivers/drv_em1xx.c)
target_sources(app PRIVATE src/drivers/drv_em5xx.c)
target_sources(app PRIVATE src/drivers/drv_flowt_ft201.c)
target_sources(app PRIVATE src/drivers/drv_iem3000.c)
target_sources(app PRIVATE src/drivers/drv_microsens_180hs.c)
target_sources(app PRIVATE src/drivers/drv_or_we_504.c)
target_sources(app PRIVATE src/drivers/drv_or_we_516.c)
target_sources(app PRIVATE src/drivers/drv_promag_mf7s.c)
target_sources(app PRIVATE src/main.c)

target_precompile_headers(app PRIVATE src/feature.h)

# ### Preserved code "cmake" (begin)
# ^^^ Preserved code "cmake" (end)
