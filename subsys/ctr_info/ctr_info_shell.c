#include <ctr_info.h>

#include <shell/shell.h>
#include <zephyr.h>

static int cmd_sn(const struct shell *shell, size_t argc, char **argv)
{
	int ret;

	uint32_t sn;

	ret = ctr_info_get_serial_number(&sn);
	if (ret < 0) {
		shell_error(shell, "failed to read serial number: %d", ret);
		return ret;
	}

	shell_print(shell, "serial number: %u", sn);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_info,
                               SHELL_CMD_ARG(sn, NULL, "Get serial number", cmd_sn, 0, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(info, &sub_info, "Info commands.", NULL);
