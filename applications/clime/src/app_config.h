#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct app_config {
	int interval_sample;
	int interval_aggregate;
	int interval_report;
};

extern struct app_config g_app_config;

int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);

int app_config_cmd_config_interval_sample(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_interval_aggregate(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_interval_report(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
