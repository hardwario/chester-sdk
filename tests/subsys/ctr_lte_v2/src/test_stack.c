
#include "mock.h"

#include <chester/ctr_lte_v2.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>
#include <zephyr/settings/settings.h>

#include "ctr_lte_v2_state.h"

// static void shell_cmd(const char *cmd)
// {
// 	const struct shell *sh = shell_backend_dummy_get_ptr();

// 	WAIT_FOR(shell_ready(sh), 20000, k_msleep(1));
// 	zassert_true(shell_ready(sh), "timed out waiting for dummy shell backend");

// 	shell_backend_dummy_clear_output(sh);

// 	int ret = shell_execute_cmd(sh, cmd);
// 	zassert_ok(ret, "shell_execute_cmd failed: %s", cmd);
// }

ZTEST(stack, test_attach)
{

	/* clang-format off */
	#define URC(ms, urc) { .rx=urc, .delay_ms=ms }


	static struct mock_link_item items[] = {
		{"AT", "OK"},
		{"AT+CGSN=1", "+CGSN: \"351358815178345\"", "OK"},
		{"AT%HWVERSION", "%HWVERSION: nRF9160 SICA B1A", "OK"},
		{"AT%SHORTSWVER", "%SHORTSWVER: nrf9160_1.3.2", "OK"},
		{"AT#XSLMVER", "#XSLMVER: \"2.5.2\",\"2.5.0-lte-5ccd2d4dd54c\"", "OK"},
		{"AT#XVERSION", "#XVERSION: \"v1.7.0\"", "OK"},
		{"AT+CFUN=0", "OK"},
		{"AT%XPOFWARN=1,30", "OK"},
		{"AT%XTEMPHIGHLVL=70", "OK"},
		{"AT%XTEMP=1", "OK"},
		{"AT%XSYSTEMMODE=0,1,1,0", "OK"},
		{"AT%XDATAPRFL=0", "OK"},
		#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
		{"AT%XBANDLOCK=1,\"0000000000000000000000100000000000000000000000000000000000001011000011110001100010011010\"", "OK"},
		#else
		{"AT%XBANDLOCK=0", "OK"},
		#endif
		{"AT%XSIM=1", "OK"},
		{"AT%XNETTIME=1", "OK"},
		{"AT%MDMEV=1", "OK"},
		{"AT%REL14FEAT=1,1,1,1,0", "OK"},
		{"AT%RAI=1", "OK"},
		{"AT+CPSMS=1,,,\"00111000\",\"00000000\"", "OK"},
		{"AT+CEPPI=1", "OK"},
		{"AT+CEREG=5", "OK"},
		{"AT+CGEREP=1", "OK"},
		{"AT+CMEE=1", "OK"},
		{"AT+CNEC=24", "OK"},
		{"AT%XCOEX0=1,1,1565,1586", "OK"},
		{"AT+CSCON=1", "OK"},
		#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
		{"AT+COPS=1,2,\"23003\"", "OK"},
		{"AT+CGDCONT=0,\"IP\",\"hardwario\"", "OK"},
		#else
		{"AT+COPS=0", "OK"},
		{"AT+CGDCONT=0,\"IP\"", "OK"},
		#endif
		{"AT+CGAUTH=0,0", "OK"},
		{"AT%XMODEMSLEEP=1,500,10240", "OK"},
		{"AT+CFUN=1", "%XMODEMSLEEP: 4", "OK"},
		URC(0, "%XMODEMSLEEP: 4,0"),
		URC(0, "+CEREG: 0"),
		URC(500, "+CEREG: 2,\"B4DC\",\"000AE520\",9"),
		URC(100, "%XSIM: 1"),
		{"AT+CIMI", "901288910017273", "OK"},
		{"AT+CIMI", "901288910017273", "OK"},
		{"AT%XICCID", "%XICCID: 89882390000262826558", "OK"},
		{"AT+CRSM=176,28539,0,0,12", "+CRSM: 144,0,\"FFFFFFFFFFFFFFFFFFFFFFFF\"", "OK"},
		URC(100, "+CEREG: 5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\""),
		{"AT+COPS?", "+COPS: 1,2,\"23003\",9", "OK"},
		{"AT%XCBAND", "%XCBAND: 20", "OK"},
		{"AT+CEINFO?", "+CEINFO: 0,1,C,8,1,-79,21", "OK"},
		{"AT+CGDCONT?", "+CGDCONT: 0,\"IP\",\"hardwario\",\"172.28.0.139\",0,0", "OK"},
		{"AT#XSOCKET=1,2,0", "#XSOCKET: 0,2,17", "OK"},
		#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
		{"AT#XCONNECT=\"46.101.214.168\",5003", "#XCONNECT: 1", "OK"},
		#else
		{"AT#XCONNECT=\"192.168.192.4\",5003", "#XCONNECT: 1", "OK"},
		#endif
	};

	/* clang-format on */

	mock_ctr_lte_link_start(items, ARRAY_SIZE(items));

	// shell_cmd("lte config autoconn true");

	// int ret = settings_load_subtree("lte");
	// zassert_equal(ret, 0, "settings_load_subtree failed");

	ctr_lte_v2_enable();

	int ret = ctr_lte_v2_wait_for_connected(K_MSEC(6000));
	zassert_ok(ret, "ctr_lte_v2_wait_for_connected failed");

	// k_sleep(K_MSEC(6000));

	// zassert_equal(2, 1, "1 should be equal to 1");
}

ZTEST_SUITE(stack, NULL, NULL, NULL, NULL, NULL);
