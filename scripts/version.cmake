#
# Copyright (c) 2024 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

# Derive firmware version from git (falls back to FW_VERSION_FALLBACK set by caller)
execute_process(
    COMMAND git describe --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(GIT_TAG)
    string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${GIT_TAG}")
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
    set(VERSION_MINOR ${CMAKE_MATCH_2})
    set(VERSION_PATCH ${CMAKE_MATCH_3})
else()
    string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${FW_VERSION_FALLBACK}")
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
    set(VERSION_MINOR ${CMAKE_MATCH_2})
    set(VERSION_PATCH ${CMAKE_MATCH_3})
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHORT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_SHORT_SHA)
    set(GIT_SHORT_SHA "unknown")
endif()

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/version.in
    ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
    @ONLY
)
