#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

/* Zephyr includes */
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CONFIG_CHANNEL_COUNT 4

#define APP_CONFIG_REPORT_INTERVAL_MIN 30
#define APP_CONFIG_REPORT_INTERVAL_MAX 86400

struct app_config {
	bool channel_active[APP_CONFIG_CHANNEL_COUNT];
	bool channel_differential[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_x0[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_y0[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_x1[APP_CONFIG_CHANNEL_COUNT];
	int channel_calib_y1[APP_CONFIG_CHANNEL_COUNT];
	int measurement_interval;
	int report_interval;
};

extern struct app_config g_app_config;

int app_config_get_report_interval(void);
int app_config_set_report_interval(int value);
int app_config_cmd_config_show(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_active(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_differential(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_x0(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_y0(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_x1(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_channel_calib_y1(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_measurement_interval(const struct shell *shell, size_t argc, char **argv);
int app_config_cmd_config_report_interval(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H_ */
