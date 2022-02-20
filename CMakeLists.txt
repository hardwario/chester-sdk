add_subdirectory(drivers)
add_subdirectory(lib)
add_subdirectory(subsys)

zephyr_include_directories(include)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CTR_BUTTON src/ctr_button.c)
zephyr_library_sources_ifdef(CONFIG_CTR_CHESTER_X0D src/ctr_chester_x0d.c)

zephyr_library_link_libraries_ifdef(CONFIG_CTR_BLE MCUMGR)
