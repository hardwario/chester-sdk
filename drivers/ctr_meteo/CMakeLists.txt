zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CTR_METEO ctr_meteo.c)
