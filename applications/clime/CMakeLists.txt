#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_ds18b20 ctr_lrw ctr_lte ctr_rtd_a ctr_rtd_b ctr_s1 ctr_s2 ctr_soil_sensor ctr_x4_a ctr_x4_b ctr_x10 ctr_z
set(SHIELD ctr_lrw ctr_lte_v2 ctr_s2)
set(ENV{FW_BUNDLE} "com.hardwario.chester.app.clime")
set(ENV{FW_NAME} "CHESTER Clime")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(clime)

add_custom_command(
    COMMAND west gen-codec -d ../codec/cbor-decoder.yaml -e ../codec/cbor-encoder.yaml -o ../src/app_codec.h
    OUTPUT ../src/app_codec.h
    DEPENDS ../codec/cbor-decoder.yaml
)

target_sources(app PRIVATE src/app_tamper.c)
target_sources(app PRIVATE src/app_backup.c)
target_sources(app PRIVATE src/app_cbor.c)
target_sources(app PRIVATE src/app_config.c)
target_sources(app PRIVATE src/app_data.c)
target_sources(app PRIVATE src/app_handler.c)
target_sources(app PRIVATE src/app_init.c)
target_sources(app PRIVATE src/app_power.c)
target_sources(app PRIVATE src/app_send.c)
target_sources(app PRIVATE src/app_sensor.c)
target_sources(app PRIVATE src/app_shell.c)
target_sources(app PRIVATE src/app_work.c)

target_sources(app PRIVATE src/main.c)

target_sources(app PRIVATE src/app_codec.h)
