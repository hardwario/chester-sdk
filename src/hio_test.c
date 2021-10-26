#include <hio_test.h>
#include <hio_bsp.h>
#include <hio_lte_talk.h>
#include <hio_lte_uart.h>

/* Zephyr includes */
#include <init.h>
#include <logging/log.h>
#include <shell/shell.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(hio_test, LOG_LEVEL_DBG);

/* TODO Delete this variable */
static atomic_t m_is_active;

static atomic_t m_is_blocked;

static struct k_poll_signal m_start_sig;

/* TODO Remove this function */
bool hio_test_is_active(void)
{
	return atomic_get(&m_is_active);
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	if (argc > 1) {
		shell_error(shell, "command not found: %s", argv[1]);
		shell_help(shell);
		return -EINVAL;
	}

	shell_help(shell);

	return 0;
}

static int cmd_test_lte_ant_int(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int ret;

	if (!atomic_get(&m_is_active)) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_rf_ant` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_test_lte_ant_ext(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int ret;

	if (!atomic_get(&m_is_active)) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	ret = hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_EXT);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_rf_ant` failed: %d", ret);
		return ret;
	}

	return 0;
}

static int cmd_test_lte_reset(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	// int ret;

	if (!atomic_get(&m_is_active)) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	/* TODO Implement */
	/*
	ret = hio_lte_uart_enable();

	if (ret < 0) {
		LOG_ERR("Call `hio_lte_uart_enable` failed: %d", ret);
		return ret;
	}

	ret = hio_bsp_set_lte_reset(0);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_lte_reset` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = hio_bsp_set_lte_reset(1);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_lte_reset` failed: %d", ret);
		return ret;
	}
	*/

	return 0;
}

static int cmd_test_lte_wakeup(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	// int ret;

	if (!atomic_get(&m_is_active)) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	/* TODO Implement */
	/*
	ret = hio_lte_uart_enable();

	if (ret < 0) {
		LOG_ERR("Call `hio_lte_uart_enable` failed: %d", ret);
		return ret;
	}

	ret = hio_bsp_set_lte_wkup(0);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_lte_wkup` failed: %d", ret);
		return ret;
	}

	k_sleep(K_MSEC(10));

	ret = hio_bsp_set_lte_wkup(1);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_lte_wkup` failed: %d", ret);
		return ret;
	}
	*/

	return 0;
}

static int cmd_test_lte_sleep(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	// int ret;

	if (!atomic_get(&m_is_active)) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	/* TODO Implement */
	/*
	ret = hio_lte_talk_cmd("AT#XSLEEP=2");

	if (ret < 0) {
		LOG_ERR("Call `hio_lte_talk_cmd` failed: %d", ret);
		return ret;
	}

	k_sleep(K_SECONDS(1));

	ret = hio_lte_uart_disable();

	if (ret < 0) {
		LOG_ERR("Call `hio_lte_uart_disable` failed: %d", ret);
		return ret;
	}
	*/

	return 0;
}

static int cmd_test_lte_cmd(const struct shell *shell, size_t argc, char **argv)
{
	// int ret;

	if (!atomic_get(&m_is_active)) {
		shell_error(shell, "test mode is not activated");
		return -ENOEXEC;
	}

	/* TODO Implement */
	/*
	if (argc > 2) {
		shell_error(shell, "only one argument is accepted (use quotes?)");
		return -EINVAL;
	}

	ret = hio_lte_talk_cmd("%s", argv[1]);

	if (ret < 0) {
		LOG_ERR("Call `hio_lte_talk_cmd` failed: %d", ret);
		return ret;
	}
	*/

	return 0;
}

static int cmd_test_start(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int ret;

	unsigned int signaled;
	int result;

	k_poll_signal_check(&m_start_sig, &signaled, &result);

	if (signaled != 0) {
		shell_warn(shell, "Test mode is already active");
		return -ENOEXEC;
	}

	if (atomic_get(&m_is_blocked)) {
		shell_error(shell, "Test mode activation is blocked");
		return -EBUSY;
	}

	k_poll_signal_raise(&m_start_sig, 0);

	/* TODO Delete this statement */
	atomic_set(&m_is_active, true);

	/* TODO Remove any initialization below */

	ret = hio_bsp_set_rf_ant(HIO_BSP_RF_ANT_INT);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_rf_ant` failed: %d", ret);
		return ret;
	}

	ret = hio_bsp_set_rf_mux(HIO_BSP_RF_MUX_LTE);

	if (ret < 0) {
		LOG_ERR("Call `hio_bsp_set_rf_mux` failed: %d", ret);
		return ret;
	}

	/* TODO Implement */
	/*
	ret = hio_lte_uart_init();

	if (ret < 0) {
		LOG_ERR("Call `hio_lte_uart_init` failed: %d", ret);
		return ret;
	}
	*/

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_test_lte_ant,
                               SHELL_CMD_ARG(int, NULL, "Use internal antenna.",
                                             cmd_test_lte_ant_int, 1, 0),
                               SHELL_CMD_ARG(ext, NULL, "Use external antenna.",
                                             cmd_test_lte_ant_ext, 1, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
        sub_test_lte, SHELL_CMD_ARG(ant, &sub_test_lte_ant, "Set antenna path.", print_help, 1, 0),
        SHELL_CMD_ARG(reset, NULL, "Reset modem.", cmd_test_lte_reset, 1, 0),
        SHELL_CMD_ARG(wakeup, NULL, "Wake up modem.", cmd_test_lte_wakeup, 1, 0),
        SHELL_CMD_ARG(sleep, NULL, "Put modem to sleep.", cmd_test_lte_sleep, 1, 0),
        SHELL_CMD_ARG(cmd, NULL, "Send AT command to modem.", cmd_test_lte_cmd, 2, 0),
        SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_test,
                               SHELL_CMD_ARG(start, NULL, "Start test mode.", cmd_test_start, 1, 0),
                               SHELL_CMD_ARG(lte, &sub_test_lte, "LTE modem commands.", print_help,
                                             1, 0),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(test, &sub_test, "Test commands.", print_help);

static int init(const struct device *dev)
{
	ARG_UNUSED(dev);

	int ret;

	LOG_INF("System initialization");

	k_poll_signal_init(&m_start_sig);

	LOG_INF("Waiting for test mode entry");

	struct k_poll_event events[] = { K_POLL_EVENT_INITIALIZER(
		K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &m_start_sig) };

	ret = k_poll(events, ARRAY_SIZE(events), K_MSEC(5000));

	if (ret == -EAGAIN) {
		LOG_INF("Test mode entry timed out");
		atomic_set(&m_is_blocked, true);
		return 0;
	}

	if (ret < 0) {
		LOG_ERR("Call `k_poll` failed: %d", ret);
		return ret;
	}

	LOG_INF("Test mode started");

	for (;;) {
		k_sleep(K_FOREVER);
	}

	return 0;
}

SYS_INIT(init, APPLICATION, CONFIG_HIO_TEST_INIT_PRIORITY);
