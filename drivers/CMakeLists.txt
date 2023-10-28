#
# Copyright (c) 2023 HARDWARIO a.s.
#
# SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
#

add_subdirectory_ifdef(CONFIG_ADC adc)
add_subdirectory_ifdef(CONFIG_CTR_A1 ctr_a1)
add_subdirectory_ifdef(CONFIG_CTR_B1 ctr_b1)
add_subdirectory_ifdef(CONFIG_CTR_BATT ctr_batt)
add_subdirectory_ifdef(CONFIG_CTR_K1 ctr_k1)
add_subdirectory_ifdef(CONFIG_CTR_LRW ctr_lrw_if)
add_subdirectory_ifdef(CONFIG_CTR_LTE_IF ctr_lte_if)
add_subdirectory_ifdef(CONFIG_CTR_LTE_IF_V2 ctr_lte_if_v2)
add_subdirectory_ifdef(CONFIG_CTR_METEO ctr_meteo)
add_subdirectory_ifdef(CONFIG_CTR_RFMUX ctr_rfmux)
add_subdirectory_ifdef(CONFIG_CTR_S1 ctr_s1)
add_subdirectory_ifdef(CONFIG_CTR_X0 ctr_x0)
add_subdirectory_ifdef(CONFIG_CTR_X3 ctr_x3)
add_subdirectory_ifdef(CONFIG_CTR_X4 ctr_x4)
add_subdirectory_ifdef(CONFIG_CTR_X7 ctr_x7)
add_subdirectory_ifdef(CONFIG_CTR_X9 ctr_x9)
add_subdirectory_ifdef(CONFIG_CTR_X10 ctr_x10)
add_subdirectory_ifdef(CONFIG_CTR_Z ctr_z)
add_subdirectory_ifdef(CONFIG_GNSS gnss)
add_subdirectory_ifdef(CONFIG_MB7066 mb7066)
add_subdirectory_ifdef(CONFIG_PEOPLE_COUNTER people_counter)
add_subdirectory_ifdef(CONFIG_SENSOR sensor)
add_subdirectory_ifdef(CONFIG_SERIAL serial)
add_subdirectory_ifdef(CONFIG_W1 w1)
