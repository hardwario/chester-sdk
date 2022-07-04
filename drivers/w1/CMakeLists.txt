zephyr_library()

zephyr_library_sources_ifdef(CONFIG_DS28E17 w1_ds28e17.c)
