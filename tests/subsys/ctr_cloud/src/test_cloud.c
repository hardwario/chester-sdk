/** @file
 *  @brief cloud transfere test suite
 */

#include "mock.h"

#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/ztest.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>

#include <chester/ctr_buf.h>
#include <chester/ctr_cloud.h>
#include <ctr_cloud_packet.h>
#include <ctr_cloud_msg.h>
#include <ctr_cloud_util.h>

ZTEST(subsus_ctr_cloud_2_cloud, test_boot)
{
	int ret;

	static char *comunication[] = {
		// send: Sequence: 0 [FLxx] len: 128 / upload session
		"422941fa2c33723f80b00001c00000bf0000101a80b00001016948415244574152494f026943484553"
		"5445522d4d036443474c53046452332e32057818636f6d2e68617264776172696f2e6d6f636b75702d"
		"617070066a6d6f636b75702d617070076676302e302e3108663635343132330a1b0003824430f85009"
		"091b00007048860ddf750b6676312e352e30ff",
		// recv: Sequence: 1 [xxAP] len: 0 / ack + signal poll
		"49247ac38dacbbf280b000013001",
		// send: Sequence: 2 [xxxP] len: 0 / poll
		"98b2f3d80b97ac9280b000011002",
		// recv: Sequence: 3 [FLxx] len: 73 / download session
		"9d810702bd57739980b00001c00380a4001a64ba538f041a64ba538e05782430313838643835632d38"
		"3936322d373836662d633938352d6136376665653762623134340672746573742d6465766963652d73"
		"302d393835",
		// send: Sequence: 4 [xxAx] len: 0 / ack
		"7cf1efb80c99381780b000012004",
		// no recv
		NULL,
		// send: Sequence: 5 [FLxx] len: 115 / upload config
		"ffd664838db07ee980b00001c0050273a9151b127641bb009f781c61707020636f6e66696720696e74"
		"657276616c2d73616d706c652035781d61707020636f6e66696720696e74657276616c2d7265706f72"
		"74203630782861707020636f6e666967206261636b75702d7265706f72742d636f6e6e656374656420"
		"66616c7365ff",
		// recv: equence: 6 [xxAP] len: 0 / ack + signal poll
		"0842b756691c629f80b000013006",
		// send: Sequence: 7 [xxxP] len: 0 / poll
		"945af7019589847480b000011007",
		// recv: Sequence: 8 [FLxx] len: 106 / download config
		"d2ea74f6a898857880b00001c008820083781d61707020636f6e66696720696e74657276616c2d7361"
		"6d706c65203130781d61707020636f6e66696720696e74657276616c2d7265706f7274203630782761"
		"707020636f6e666967206261636b75702d7265706f72742d636f6e6e65637465642074727565",
		// send: Sequence: 9 [xxAx] len: 0 / ack
		"80e388a08e84beb080b000012009",
		// no recv
		NULL,
		// send: Sequence: 10 [xxxP] len: 0
		"cb4bb2d99e3a56e680b00001100a",
		// recv: Sequence: 11 [FLxx] len: 0
		"7bc147fda33d7aee80b00001c00b",
		// send: packet Sequence: 12 [xxxP] len: 0
		"ae81ae8eeedbd27180b00001100c",
		// recv: Sequence: 13 [FLxx] len: 66 / download shell
		"76e10e221e84573080b00001c00d87a200826f61707020636f6e6669672073686f77781a6170702063"
		"6f6e66696720696e74657276616c2d73616d706c650150018dd0f29d0175f2abaf092b8d2f9f2b",
		// send: Sequence: 14 [xxAx] len: 0
		"5eded0b6ce8631e880b00001200e",
		NULL,
		// send: Sequence: 15 [FLxx] len: 259 / upload shell
		"327c0f4de53ae7ed80b00001c00f07bf0150018dd0f29d0175f2abaf092b8d2f9f2b009fbf006f6170"
		"7020636f6e6669672073686f77029f781d61707020636f6e66696720696e74657276616c2d73616d70"
		"6c65203130781d61707020636f6e66696720696e74657276616c2d7265706f72742036307827617070"
		"20636f6e666967206261636b75702d7265706f72742d636f6e6e65637465642074727565ffffbf0078"
		"1a61707020636f6e66696720696e74657276616c2d73616d706c650135029f7848696e74657276616c"
		"2d73616d706c65202d204765742f5365742073616d706c6520696e74657276616c20696e207365636f"
		"6e64732028666f726d61743a203c352d333630303e292effffffff",
		// recv: Sequence: 16 [xxAx] len: 0
		"ca3e3abdaf98f36880b000012010",
		NULL,
	};

	mock_ctr_lte_v2_set_send_recv(comunication, sizeof(comunication) / sizeof(comunication[0]));

	static struct ctr_cloud_options options = {
		.decoder_hash = 0,
		.encoder_hash = 0,
		.decoder_buf = NULL,
		.decoder_len = 0,
		.encoder_buf = NULL,
		.encoder_len = 0,
	};

	int64_t ts;

	// ret = ctr_cloud_get_last_seen_ts(&ts);
	// zassert_equal(ret, -EPERM, "ctr_cloud_get_last_seen_ts failed");

	ret = ctr_cloud_init(&options);
	zassert_ok(ret, "ctr_cloud_init failed");

	ret = ctr_cloud_wait_initialized(K_MSEC(2000));
	zassert_ok(ret, "ctr_cloud_wait_initialized failed");

	ret = ctr_cloud_get_last_seen_ts(&ts);
	zassert_ok(ret, "ctr_cloud_get_last_seen_ts");

	zassert_equal(ts, 1689932686, "last downlink ts");

	printf("\ntest: last downlink ts: %lld\n", ts);

	ctr_cloud_downlink(K_FOREVER); // poll without data
	ctr_cloud_downlink(K_FOREVER); // poll shell

	k_sleep(K_MSEC(300));
}

// ZTEST(subsus_ctr_cloud_2_cloud, test_app_fw)
// {
// 	int ret;

// 	static char *comunication[] = {
// 		// send: Sequence: 0 [FLxx] len: 128 / upload session
// 		"422941fa2c33723f80b00001c00000bf0000101a80b00001016948415244574152494f026943484553"
// 		"5445522d4d036443474c53046452332e32057818636f6d2e68617264776172696f2e6d6f636b75702d"
// 		"617070066a6d6f636b75702d617070076676302e302e3108663635343132330a1b0003824430f85009"
// 		"091b00007048860ddf750b6676312e352e30ff",
// 		// recv: Sequence: 1 [xxAP] len: 0 / ack + signal poll
// 		"49247ac38dacbbf280b000013001",
// 		// send: Sequence: 2 [xxxP] len: 0 / poll
// 		"98b2f3d80b97ac9280b000011002",
// 		// recv: Sequence: 3 [FLxx] len: 93 / download session
// 		"ea9c3efa69351cef80b00001c00380a6001a65e62f3d021b46c3ba674e0f604f031b73a9151b127641"
// 		"bb041a65e62f3c05782430313838643835632d383936322d373836662d633938352d61363766656537"
// 		"62623134340672746573742d6465766963652d73302d393835",
// 		// send: Sequence: 4 [xxAx] len: 0 / ack
// 		"7cf1efb80c99381780b000012004",
// 		// no recv
// 		NULL,
// 		// Sequence: 5 [xxxP] len: 0
// 		"8518c99e482c211680b000011005",
// 		// Sequence: 6 [FLxx] len: 0
// 		"fdca48f52dcd34e580b00001c006",
// 		// Sequence: 7 [FLxx] len: 57 / upload firmware app start
// 		"d525afc2f3d822eb80b00001c00708bf00636170700168646f776e6c6f616404193f00057820613861"
// 		"3839663530386130383462323239393634316266346135306636356564ff",
// 		// Sequence: 8 [xxAP] len: 0
// 		"f595fffcda87190080b000013008",
// 		// Sequence: 9 [xxxP] len: 0
// 		"e4033647654ea95f80b000011009",
// 		// Sequence: 10 [FLxx] len: 73 // download firmware chunk offset 0
// 		"2f378122505c8de480b00001c00a88a7006361707001656368756e6b0250018e39b69e7d73d68497b2"
// 		"9fb053ba20030004181e05581eaabbccddee00112233445566778899aabbccddee0011223344556677"
// 		"889906181e",
// 		// Sequence: 11 [xxAx] len: 0
// 		"c858daa739bdd2fb80b00001200b",
// 		// no recv
// 		NULL,
// 	};
// 	mock_ctr_lte_v2_set_send_recv(comunication, sizeof(comunication) / sizeof(comunication[0]));

// 	static struct ctr_cloud_options options = {
// 		.decoder_hash = 0,
// 		.encoder_hash = 0,
// 		.decoder_buf = NULL,
// 		.decoder_len = 0,
// 		.encoder_buf = NULL,
// 		.encoder_len = 0,
// 	};

// 	ret = ctr_cloud_init(&options);
// 	zassert_ok(ret, "ctr_cloud_init failed");

// 	ret = ctr_cloud_wait_initialized(K_MSEC(2000));
// 	zassert_ok(ret, "ctr_cloud_wait_initialized failed");

// 	ctr_cloud_recv(); // poll without data

// 	const struct shell *sh = shell_backend_dummy_get_ptr();
// 	shell_backend_dummy_clear_output(sh);
// 	ret = shell_execute_cmd(sh, "cloud firmware download a8a89f508a084b2299641bf4a50f65ed");
// 	// zassert_ok(ret, "shell_execute_cmd failed");
// 	size_t size;
// 	const char *p = shell_backend_dummy_get_output(sh, &size);
// 	zassert_not_null(p, "shell_backend_dummy_get_output failed");

// 	printf("\n%s\n", p);

// 	k_sleep(K_MSEC(500));
// }

ZTEST_SUITE(subsus_ctr_cloud_2_cloud, NULL, mock_global_setup_suite, NULL, NULL, NULL);
