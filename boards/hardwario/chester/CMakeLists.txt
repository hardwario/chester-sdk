if(CONFIG_MCUBOOT_ACTION_HOOKS)
  zephyr_library()
  zephyr_library_sources(mcuboot_hook.c)
endif()
