zephyr_library()

zephyr_library_sources(ctr_therm.c)
zephyr_library_sources_ifdef(CONFIG_CTR_THERM_SHELL ctr_therm_shell.c)
