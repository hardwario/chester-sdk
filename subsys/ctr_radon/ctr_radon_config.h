#ifndef CTR_RADON_CONFIG_H_
#define CTR_RADON_CONFIG_H_

#include <zephyr/drivers/uart.h>
#include <zephyr/shell/shell.h>

struct ctr_radon_config {
	int modbus_baud;
	int modbus_addr;
	enum uart_config_parity modbus_parity;
	enum uart_config_stop_bits modbus_stop_bits;
};

extern struct ctr_radon_config g_ctr_radon_config;

int ctr_radon_config_cmd_show(const struct shell *shell, size_t argc, char **argv);
int ctr_radon_config_cmd(const struct shell *shell, size_t argc, char **argv);
int ctr_radon_config_init(void);

#endif /* CTR_RADON_CONFIG_H_ */