zephyr_library()

zephyr_library_sources(ctr_ble.c)
zephyr_link_libraries_ifdef(CONFIG_MCUMGR MCUMGR)
