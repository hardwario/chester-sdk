cmake_minimum_required(VERSION 3.20.0)

# Supported shields: ctr_lte (ctr_mb7066_a|ctr_mb7066_b) [ctr_s2] [ctr_z] [ctr_ds18b20]
set(SHIELD ctr_ds18b20 ctr_lte ctr_mb7066_a)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(range)

target_sources(app PRIVATE
  src/app_backup.c
  src/app_cbor.c
  src/app_config.c
  src/app_data.c
  src/app_handler.c
  src/app_init.c
  src/app_power.c
  src/app_send.c
  src/app_sensor.c
  src/app_shell.c
  src/app_work.c
  src/main.c)
