#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

set(SHIELD ctr_lte_v2)
set(ENV{FW_BUNDLE} "com.hardwario.chester.app.demo")
set(ENV{FW_NAME} "CHESTER Demo")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app)

add_custom_command(
    COMMAND west gen-codec -d ../codec/cbor-decoder.yaml -e ../codec/cbor-encoder.yaml -o ../src/app_codec.h
    OUTPUT ../src/app_codec.h
    DEPENDS ../codec/cbor-decoder.yaml ../codec/cbor-encoder.yaml
)

target_sources(app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_power.c)
target_sources(app PRIVATE src/app_sensor.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/app_work.c)

target_sources(app PRIVATE src/main.c)

target_sources(app PRIVATE src/app_codec.h)