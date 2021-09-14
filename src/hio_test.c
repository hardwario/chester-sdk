#include <hio_test.h>
#include <hio_bsp.h>
#include <hio_lte_talk.h>
#include <hio_lte_uart.h>

// Zephyr includes
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_test, LOG_LEVEL_DBG);

static bool active;
static bool blocked;

void
hio_test_block(void)
{
    blocked = true;
}

bool
hio_test_is_active(void)
{
    return active;
}

static int
print_help(const struct shell *shell,
           size_t argc, char **argv)
{
	int ret = 0;

	if (argc > 1) {
		shell_error(shell, "%s: Command not found", argv[1]);
		ret = -1;
	}

	shell_help(shell);

	return ret;
}

static int
cmd_test_lte_ant_int(const struct shell *shell,
                     size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT) < 0) {
        LOG_ERR("Call `hio_bsp_set_rf_ant` failed");
        return -2;
    }

    return 0;
}

static int
cmd_test_lte_ant_ext(const struct shell *shell,
                     size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_EXT) < 0) {
        LOG_ERR("Call `hio_bsp_set_rf_ant` failed");
        return -2;
    }

    return 0;
}

static int
cmd_test_lte_reset(const struct shell *shell,
                   size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_lte_uart_enable() < 0) {
        LOG_ERR("Call `hio_lte_uart_enable` failed");
        return -2;
    }

    if (hio_bsp_set_lte_reset(0) < 0) {
        LOG_ERR("Call `hio_bsp_set_lte_reset` failed");
        return -3;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(10));

    if (hio_bsp_set_lte_reset(1) < 0) {
        LOG_ERR("Call `hio_bsp_set_lte_reset` failed");
        return -3;
    }

    return 0;
}

static int
cmd_test_lte_wakeup(const struct shell *shell,
                    size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_lte_uart_enable() < 0) {
        LOG_ERR("Call `hio_lte_uart_enable` failed");
        return -3;
    }

    if (hio_bsp_set_lte_wkup(0) < 0) {
        LOG_ERR("Call `hio_bsp_set_lte_wkup` failed");
        return -4;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(10));

    if (hio_bsp_set_lte_wkup(1) < 0) {
        LOG_ERR("Call `hio_bsp_set_lte_wkup` failed");
        return -5;
    }

    return 0;
}

static int
cmd_test_lte_sleep(const struct shell *shell,
                   size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_lte_talk_cmd("AT#XSLEEP=2") < 0) {
        LOG_ERR("Call `hio_lte_talk_cmd` failed");
        return -2;
    }

    hio_sys_task_sleep(HIO_SYS_SECONDS(1));

    if (hio_lte_uart_disable() < 0) {
        LOG_ERR("Call `hio_lte_uart_disable` failed");
        return -3;
    }

    return 0;
}

static int
cmd_test_lte_cmd(const struct shell *shell,
                 size_t argc, char **argv)
{
    if (!active) {
        shell_error(shell, "Test mode is not activated");
        return -1;
    }

    if (argc > 2) {
        shell_error(shell, "Only one argument is allowed (use quotes?)");
        return -2;
    }

    if (hio_lte_talk_cmd("%s", argv[1]) < 0) {
        LOG_ERR("Call `hio_lte_talk_cmd` failed");
        return -3;
    }

    return 0;
}

static int
cmd_test_start(const struct shell *shell,
         size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (active) {
        shell_warn(shell, "Test mode is already active");
        return -1;
    }

    if (blocked) {
        shell_error(shell, "Test mode activation is blocked");
        return -2;
    }

    active = true;

    LOG_INF("Test mode started");

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT) < 0) {
        LOG_ERR("Call `hio_bsp_set_rf_ant` failed");
        return -3;
    }

    if (hio_bsp_set_rf_mux(HIO_BSP_RF_MUX_LTE) < 0) {
        LOG_ERR("Call `hio_bsp_set_rf_mux` failed");
        return -4;
    }

    if (hio_lte_uart_init() < 0) {
        LOG_ERR("Call `hio_lte_uart_init` failed");
        return -5;
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_test_lte_ant,
    SHELL_CMD_ARG(int, NULL, "Use internal antenna.",
                  cmd_test_lte_ant_int, 1, 0),
    SHELL_CMD_ARG(ext, NULL, "Use external antenna.",
                  cmd_test_lte_ant_ext, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_test_lte,
    SHELL_CMD_ARG(ant, &sub_test_lte_ant, "Set antenna path.",
                  print_help, 1, 0),
    SHELL_CMD_ARG(reset, NULL, "Reset modem.",
                  cmd_test_lte_reset, 1, 0),
    SHELL_CMD_ARG(wakeup, NULL, "Wake up modem.",
                  cmd_test_lte_wakeup, 1, 0),
    SHELL_CMD_ARG(sleep, NULL, "Sleep modem.",
                  cmd_test_lte_sleep, 1, 0),
    SHELL_CMD_ARG(cmd, NULL, "Send AT command to modem.",
                  cmd_test_lte_cmd, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_test,
    SHELL_CMD_ARG(start, NULL, "Start test mode.",
                  cmd_test_start, 1, 0),
    SHELL_CMD_ARG(lte, &sub_test_lte, "LTE modem commands.",
                  print_help, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(test, &sub_test, "Test commands", print_help);
