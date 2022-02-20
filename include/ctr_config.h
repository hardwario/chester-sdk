#ifndef CHESTER_INCLUDE_CTR_CONFIG_H_
#define CHESTER_INCLUDE_CTR_CONFIG_H_

/* Zephyr includes */
#include <shell/shell.h>

/* Standard include */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ctr_config_show_cb)(const struct shell *shell, size_t argc, char **argv);

void ctr_config_append_show(const char *name, ctr_config_show_cb cb);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_INCLUDE_CTR_CONFIG_H_ */
