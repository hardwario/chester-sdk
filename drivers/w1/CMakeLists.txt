zephyr_library()

zephyr_library_sources(w1_net_intern.c)
zephyr_library_sources(w1_net_public.c)

zephyr_library_sources_ifdef(CONFIG_W1_DS2484 w1_ds2484.c)
