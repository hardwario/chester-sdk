#ifndef HIO_CONFIG_H
#define HIO_CONFIG_H

// Zephyr includes
#include <shell/shell.h>

// Standard include
#include <stddef.h>

typedef int (*hio_config_show_cb)(const struct shell *shell, size_t argc, char **argv);

void hio_config_append_show(const char *name, hio_config_show_cb cb);

#endif
