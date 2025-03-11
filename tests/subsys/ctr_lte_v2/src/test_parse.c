/** @file
 *  @brief ctr_lte_v2 parse test suite
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <chester/ctr_buf.h>
#include <ctr_lte_v2_parse.h>

ZTEST(parser, test_xsocket_set)
{
	struct xsocket_set_param param;
	int ret = ctr_lte_v2_parse_xsocket_set("0,2,17", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_xsocket_set failed");
	zassert_equal(param.handle, 0, "handle not equal");
	zassert_equal(param.type, 2, "type not equal");
	zassert_equal(param.protocol, 17, "protocol not equal");
}

ZTEST(parser, test_xsocket_get)
{
	struct xsocket_get_param param;
	int ret = ctr_lte_v2_parse_xsocket_get("0,1,0,2,0", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_xsocket_set failed");
	zassert_equal(param.handle, 0, "handle not equal");
	zassert_equal(param.family, 1, "family not equal");
	zassert_equal(param.role, 0, "role not equal");
	zassert_equal(param.type, 2, "type not equal");
	zassert_equal(param.cid, 0, "cid not equal");
}

ZTEST(parser, test_urc_cereg_5)
{
	struct ctr_lte_v2_cereg_param param;
	int ret = ctr_lte_v2_parse_urc_cereg(
		"5,\"AF66\",\"009DE067\",9,,,\"00000000\",\"00111000\"", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_cereg failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.stat, 5, "param.stat not equal");
	zassert_true(strcmp(param.tac, "AF66") == 0, "param.tac not equal");
	zassert_equal(param.cid, 0x009DE067, "param.cid not equal");
	zassert_equal(param.act, 9, "param.act not equal");
	zassert_equal(param.cause_type, 0, "param.cause_type not equal");
	zassert_equal(param.reject_cause, 0, "param.reject_cause not equal");
	zassert_equal(param.active_time, 0, "param.active_time not equal");
	zassert_equal(param.periodic_tau_ext, 86400, "param.periodic_tau_ext not equal");
}

ZTEST(parser, test_urc_cereg_5_active_time)
{
	struct ctr_lte_v2_cereg_param param;
	int ret = ctr_lte_v2_parse_urc_cereg(
		"5,\"AF66\",\"009DE067\",9,,,\"00000101\",\"00111000\"", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_cereg failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.stat, 5, "param.stat not equal");
	zassert_true(strcmp(param.tac, "AF66") == 0, "param.tac not equal");
	zassert_equal(param.cid, 0x009DE067, "param.cid not equal");
	zassert_equal(param.act, 9, "param.act not equal");
	zassert_equal(param.cause_type, 0, "param.cause_type not equal");
	zassert_equal(param.reject_cause, 0, "param.reject_cause not equal");
	zassert_equal(param.active_time, 10, "param.active_time not equal");
	zassert_equal(param.periodic_tau_ext, 86400, "param.periodic_tau_ext not equal");
}

ZTEST(parser, test_urc_cereg_5_psm_disabled)
{
	struct ctr_lte_v2_cereg_param param;
	int ret = ctr_lte_v2_parse_urc_cereg(
		"5,\"3866\",\"074FEB0C\",7,,,\"11100000\",\"11100000\"", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_cereg failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.stat, 5, "param.stat not equal");
	zassert_true(strcmp(param.tac, "3866") == 0, "param.tac not equal");
	zassert_equal(param.cid, 0x074FEB0C, "param.cid not equal");
	zassert_equal(param.act, 7, "param.act not equal");
	zassert_equal(param.cause_type, 0, "param.cause_type not equal");
	zassert_equal(param.reject_cause, 0, "param.reject_cause not equal");
	zassert_equal(param.active_time, -1, "param.active_time not equal");
	zassert_equal(param.periodic_tau_ext, -1, "param.periodic_tau_ext not equal");
}

ZTEST(parser, test_urc_cereg_2)
{
	struct ctr_lte_v2_cereg_param param;
	int ret = ctr_lte_v2_parse_urc_cereg("2,\"B4DC\",\"000AE520\",9", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_cereg failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.stat, 2, "param.stat not equal");
	zassert_true(strcmp(param.tac, "B4DC") == 0, "param.tac not equal");
	zassert_equal(param.cid, 0x000AE520, "param.cid not equal");
	zassert_equal(param.act, 9, "param.act not equal");
}

ZTEST(parser, test_urc_cereg_2_empty)
{
	struct ctr_lte_v2_cereg_param param;
	int ret = ctr_lte_v2_parse_urc_cereg("2", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_cereg failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.stat, 2, "param.stat not equal");
	zassert_true(strcmp(param.tac, "") == 0, "param.tac not equal");
	zassert_equal(param.cid, 0, "param.cid not equal");
	zassert_equal(param.act, 0, "param.act not equal");
}

ZTEST(parser, test_urc_cereg_4)
{
	struct ctr_lte_v2_cereg_param param;
	int ret = ctr_lte_v2_parse_urc_cereg("4", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_cereg failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.stat, 4, "param.stat not equal");
	zassert_true(strcmp(param.tac, "") == 0, "param.tac not equal");
	zassert_equal(param.cid, 0, "param.cid not equal");
	zassert_equal(param.act, 0, "param.act not equal");
}

ZTEST(parser, test_urc_xmodemsleep_1_89999825)
{
	int p1, p2;
	int ret = ctr_lte_v2_parse_urc_xmodemsleep("1,89999825", &p1, &p2);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_xmodemsleep failed");
	zassert_equal(p1, 1, "p1 not equal");
	zassert_equal(p2, 89999825, "p2 not equal");
}

ZTEST(parser, test_urc_xmodemsleep_1_0)
{
	int p1, p2;
	int ret = ctr_lte_v2_parse_urc_xmodemsleep("1,0", &p1, &p2);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_xmodemsleep failed");
	zassert_equal(p1, 1, "p1 not equal");
	zassert_equal(p2, 0, "p2 not equal");
}

ZTEST(parser, test_urc_xmodemsleep_4_0)
{
	int p1, p2;
	int ret = ctr_lte_v2_parse_urc_xmodemsleep("4,0", &p1, &p2);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_xmodemsleep failed");
	zassert_equal(p1, 4, "p1 not equal");
	zassert_equal(p2, 0, "p2 not equal");
}

ZTEST(parser, test_urc_xmodemsleep_4)
{
	int p1, p2;
	int ret = ctr_lte_v2_parse_urc_xmodemsleep("4", &p1, &p2);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_xmodemsleep failed");
	zassert_equal(p1, 4, "p1 not equal");
	zassert_equal(p2, 0, "p2 not equal");
}

ZTEST(parser, test_coneval)
{

	struct ctr_lte_v2_conn_param param;
	int ret = ctr_lte_v2_parse_coneval(
		"0,1,7,68,29,47,\"000AE520\",\"23003\",135,6447,20,0,0,14,2,1,99", &param);

	zassert_ok(ret, "ctr_lte_v2_parse_coneval failed");
	zassert_equal(param.valid, true, "param.valid not true");
	zassert_equal(param.result, 0, "param.result not equal"); // success
	zassert_equal(param.eest, 7, "param.eest not equal");     // energy_estimate
	zassert_equal(param.ecl, 0, "param.ecl not equal");
	zassert_equal(param.rsrp, -72, "param.rsrp not equal");
	zassert_equal(param.rsrq, -5, "param.rsrq not equal");
	zassert_equal(param.snr, 23, "param.snr not equal");
	zassert_equal(param.plmn, 23003, "param.plmn not equal");
	zassert_equal(param.cid, 0x000AE520, "param.cid not equal");
	zassert_equal(param.band, 20, "param.band not equal");
	zassert_equal(param.earfcn, 6447, "param.earfcn not equal");
}

ZTEST(parser, test_urc_xgps)
{
	struct ctr_lte_v2_gnss_update update;

	char line[] = "49.256683,17.699627,292.599670,5.468742,0.165512,73.682823,\"2024-06-27 "
		      "16:06:52\"";
	int ret = ctr_lte_v2_parse_urc_xgps(line, &update);

	zassert_ok(ret, "ctr_lte_v2_parse_urc_xgps failed");
	zassert_within((double)update.latitude, 49.256683, 0.000001, "update.latitude not equal");
	zassert_within((double)update.longitude, 17.699627, 0.000001, "update.longitude not equal");
	zassert_within((double)update.altitude, 292.599670, 0.000001, "update.altitude not equal");
	zassert_within((double)update.accuracy, 5.468742, 0.000001, "update.accuracy not equal");
	zassert_within((double)update.speed, 0.165512, 0.000001, "update.speed not equal");
	zassert_within((double)update.heading, 73.682823, 0.000001, "update.heading not equal");
	zassert_true(strcmp(update.datetime, "2024-06-27 16:06:52") == 0,
		     "update.datetime not equal");
}

ZTEST_SUITE(parser, NULL, NULL, NULL, NULL, NULL);

// int main() {
//     // Testovací příklady
//     const char *test1 = "11100000"; // Očekávaný výsledek: deaktivováno (-1)
//     const char *test2 = "00000000"; // Očekávaný výsledek: 0 sekund
//     const char *test3 = "00000101"; // GPRS Timer 2: 5 * 2 = 10 sekund
//     const char *test4 = "00101001"; // GPRS Timer 2: 9 * 60 = 540 sekund (9 minut)
//     const char *test5 = "10000101"; // GPRS Timer 3: 5 * 30 = 150 sekund (2,5 minuty)
//     const char *test6 = "11001000"; // GPRS Timer 3: 8 * 320h = 2560 hodin (deaktivováno?)

//     printf("GPRS Timer 2 - Test 1: %d\n", parse_gprs_timer(test1, 2));
//     printf("GPRS Timer 2 - Test 2: %d\n", parse_gprs_timer(test2, 2));
//     printf("GPRS Timer 2 - Test 3: %d\n", parse_gprs_timer(test3, 2));
//     printf("GPRS Timer 2 - Test 4: %d\n", parse_gprs_timer(test4, 2));

//     printf("GPRS Timer 3 - Test 5: %d\n", parse_gprs_timer(test5, 3));
//     printf("GPRS Timer 3 - Test 6: %d\n", parse_gprs_timer(test6, 3));

//     return 0;
// }
