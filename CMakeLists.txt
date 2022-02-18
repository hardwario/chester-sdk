add_subdirectory(drivers)
add_subdirectory(subsys)

zephyr_include_directories(include)

zephyr_library()

add_subdirectory(lib)

zephyr_library_sources_ifdef(CONFIG_CTR_BUTTON src/ctr_button.c)
zephyr_library_sources_ifdef(CONFIG_CTR_CHESTER_X0D src/ctr_chester_x0d.c)
zephyr_library_sources_ifdef(CONFIG_CTR_CONFIG src/ctr_config.c)
zephyr_library_sources_ifdef(CONFIG_CTR_RTC src/ctr_rtc.c)
zephyr_library_sources_ifdef(CONFIG_CTR_SDK src/ctr_buf.c)
zephyr_library_sources_ifdef(CONFIG_CTR_TEST src/ctr_test.c)

zephyr_library_link_libraries_ifdef(CONFIG_CTR_BLE MCUMGR)
