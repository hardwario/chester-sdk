#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CTR_LED ctr_led.c)
zephyr_library_sources_ifdef(CONFIG_CTR_LED_SHELL ctr_led_shell.c)
