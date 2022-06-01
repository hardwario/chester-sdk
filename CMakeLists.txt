add_subdirectory(drivers)
add_subdirectory(lib)
add_subdirectory(subsys)

zephyr_include_directories(include)

zephyr_library()

zephyr_library_link_libraries_ifdef(CONFIG_CTR_BLE MCUMGR)
