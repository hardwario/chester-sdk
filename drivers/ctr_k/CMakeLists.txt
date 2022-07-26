zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CTR_K ctr_k.c)
