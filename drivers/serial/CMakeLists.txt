zephyr_library()

zephyr_library_sources_ifdef(CONFIG_UART_SC16IS7XX uart_sc16is7xx.c)
