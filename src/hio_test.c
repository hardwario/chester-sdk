#include <hio_test.h>
#include <hio_bsp.h>
#include <hio_log.h>
#include <hio_lte_talk.h>
#include <hio_lte_uart.h>
#include <hio_util.h>

// Zephyr includes
#include <shell/shell.h>
#include <zephyr.h>

HIO_LOG_REGISTER(hio_test, HIO_LOG_LEVEL_DBG);

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
    HIO_ARG_UNUSED(argc);
    HIO_ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT) < 0) {
        hio_log_err("Call `hio_bsp_set_rf_ant` failed");
        return -2;
    }

    return 0;
}

static int
cmd_test_lte_ant_ext(const struct shell *shell,
                     size_t argc, char **argv)
{
    HIO_ARG_UNUSED(argc);
    HIO_ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_EXT) < 0) {
        hio_log_err("Call `hio_bsp_set_rf_ant` failed");
        return -2;
    }

    return 0;
}

static int
cmd_test_lte_reset(const struct shell *shell,
                   size_t argc, char **argv)
{
    HIO_ARG_UNUSED(argc);
    HIO_ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_bsp_set_lte_reset(0) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_reset` failed");
        return -2;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(10));

    if (hio_bsp_set_lte_reset(1) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_reset` failed");
        return -3;
    }

    return 0;
}

static int
cmd_test_lte_wakeup(const struct shell *shell,
                    size_t argc, char **argv)
{
    HIO_ARG_UNUSED(argc);
    HIO_ARG_UNUSED(argv);

    if (!active) {
        shell_print(shell, "Test mode is not activated");
        return -1;
    }

    if (hio_bsp_set_lte_wkup(0) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_wkup` failed");
        return -2;
    }

    hio_sys_task_sleep(HIO_SYS_MSEC(10));

    if (hio_bsp_set_lte_wkup(1) < 0) {
        hio_log_err("Call `hio_bsp_set_lte_wkup` failed");
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
        hio_log_err("Call `hio_lte_talk_cmd` failed");
        return -3;
    }

    return 0;
}

static int
cmd_test_start(const struct shell *shell,
         size_t argc, char **argv)
{
    HIO_ARG_UNUSED(argc);
    HIO_ARG_UNUSED(argv);

    if (active) {
        shell_warn(shell, "Test mode is already active");
        return -1;
    }

    if (blocked) {
        shell_error(shell, "Test mode activation is blocked");
        return -2;
    }

    active = true;

    hio_log_inf("Test mode started");

    if (hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT) < 0) {
        hio_log_fat("Call `hio_bsp_set_rf_ant` failed");
        return -3;
    }

    if (hio_bsp_set_rf_mux(HIO_BSP_RF_MUX_LTE) < 0) {
        hio_log_fat("Call `hio_bsp_set_rf_mux` failed");
        return -4;
    }

    if (hio_lte_uart_init() < 0) {
        hio_log_fat("Call `hio_lte_uart_init` failed");
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

/*

kernel reboot warm

<push button>

test start
test lte ant int
test lte reset
test lte wakeup
test lte cmd AT%XPOFWARN=1,30
test lte cmd AT%XSYSTEMMODE=0,1,0,0
test lte cmd AT%REL14FEAT=1,1,1,1,0
test lte cmd AT%XDATAPRFL=0
test lte cmd AT%XSIM=1
test lte cmd AT%XTIME=1
test lte cmd AT+CEREG=5
test lte cmd AT+CGEREP=1
test lte cmd AT+CSCON=1
test lte cmd AT+CPSMS=1,,,\"00111000\",\"00000000\"
test lte cmd AT+CNEC=24
test lte cmd AT+CMEE=1
test lte cmd AT+CFUN=1

*/
