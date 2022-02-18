zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CTR_BUF ctr_buf.c)
