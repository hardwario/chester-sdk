zephyr_library()

zephyr_library_sources_ifdef(CONFIG_GPIO_PCAL6416A gpio_pcal6416a.c)
