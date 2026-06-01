#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

add_subdirectory_ifdef(CONFIG_MPL3115A2 mpl3115a2)
add_subdirectory_ifdef(CONFIG_SPS30 sps30)
add_subdirectory_ifdef(CONFIG_HIO_TMP112 tmp112)
