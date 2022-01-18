zephyr_library()

zephyr_library_sources_ifdef(CONFIG_ADC_TLA2021 adc_tla2021.c)
