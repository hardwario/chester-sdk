#
# Copyright (c) 2026 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

include_guard()

set(APP_CODEC_DIR ${PROJECT_BINARY_DIR}/include/generated/app)
set(APP_CODEC_DECODER ${CMAKE_CURRENT_SOURCE_DIR}/codec/cbor-decoder.yaml)
set(APP_CODEC_ENCODER ${CMAKE_CURRENT_SOURCE_DIR}/codec/cbor-encoder.yaml)

set(APP_CODEC_ARGS -d ${APP_CODEC_DECODER})
set(APP_CODEC_DEPS ${APP_CODEC_DECODER})

if(EXISTS ${APP_CODEC_ENCODER})
    list(APPEND APP_CODEC_ARGS -e ${APP_CODEC_ENCODER})
    list(APPEND APP_CODEC_DEPS ${APP_CODEC_ENCODER})
endif()

add_custom_command(
    OUTPUT ${APP_CODEC_DIR}/app_codec.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${APP_CODEC_DIR}
    COMMAND ${WEST} gen-codec ${APP_CODEC_ARGS} -o ${APP_CODEC_DIR}/app_codec.h
    DEPENDS ${APP_CODEC_DEPS}
    VERBATIM
)

add_custom_target(app_codec_h DEPENDS ${APP_CODEC_DIR}/app_codec.h)

zephyr_include_directories(${APP_CODEC_DIR})
add_dependencies(zephyr_generated_headers app_codec_h)
