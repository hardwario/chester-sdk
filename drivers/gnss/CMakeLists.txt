zephyr_library()

zephyr_library_sources_ifdef(CONFIG_GNSS_M8 gnss_m8.c)
