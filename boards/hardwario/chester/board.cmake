#
# Copyright (c) 2024 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

if(CONFIG_BOARD_CHESTER_NRF52840)
  board_runner_args(jlink "--device=nRF52840_xxAA" "--speed=4000")
  board_runner_args(pyocd "--target=nrf52840" "--frequency=4000000")
  include(${ZEPHYR_BASE}/boards/common/nrfjprog.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/nrfutil.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
  include(${ZEPHYR_BASE}/boards/common/openocd-nrf5.board.cmake)
endif()
