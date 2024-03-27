/** @file
 *  @brief cloud message test suite
 *
 */

#include "helper.h"
#include "mock.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/settings/settings.h>

#include <chester/ctr_buf.h>
#include <ctr_cloud_msg.h>
#include <ctr_cloud_util.h>

ZTEST(subsus_ctr_cloud_1_msg, test_pack_create_session)
{
	CTR_BUF_DEFINE(buffer, 512);

	int ret = ctr_cloud_msg_pack_create_session(&buffer);
	zassert_ok(ret, "ctr_cloud_msg_pack_create_session failed");

	char *expectHex = "00bf0000101a80b00001016948415244574152494f0269434845535445522d4d03644347"
			  "4c53046452332e32057818636f6d2e68617264776172696f2e6d6f636b75702d61707006"
			  "6a6d6f636b75702d617070076676302e302e3108663635343132330a1b0003824430f850"
			  "09091b00007048860ddf750b6676312e352e30ff";

	CTR_BUF_DEFINE(expect, 512);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");
	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_unpack_set_session)
{
	CTR_BUF_DEFINE(buffer, 512);

	char *input = "80a6001a649e9358011b1fbe3ac758c6f88c021b46c3ba674e0f604f041a649e935905782430"
		      "313839306236362d633866352d373065612d396634332d61666532346564663966353506696b"
		      "6172656c2d646576";

	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);

	struct ctr_cloud_session session;

	int ret = ctr_cloud_msg_unpack_set_session(&buffer, &session);
	zassert_ok(ret, "ctr_cloud_msg_unpack_set_session failed");

	zassert_equal(session.id, (uint32_t)1688114008, "id");
	zassert_equal(session.decoder_hash, (uint64_t)0x1fbe3ac758c6f88c, "decoder_hash");
	zassert_equal(session.encoder_hash, (uint64_t)0x46c3ba674e0f604f, "encoder_hash");
	zassert_equal(session.config_hash, 0, "config_hash");
	zassert_true(strcmp(session.device_id, "01890b66-c8f5-70ea-9f43-afe24edf9f55") == 0, "id");
	zassert_true(strcmp(session.device_name, "karel-dev") == 0, "name");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_get_timestamp)
{
	CTR_BUF_DEFINE(buffer, 512);

	int ret = ctr_cloud_msg_pack_get_timestamp(&buffer);
	zassert_ok(ret, "ctr_cloud_msg_pack_get_timestamp failed");

	uint8_t expect[] = {0x01};

	zassert_equal(buffer.len, 1, "len equal");
	zassert_mem_equal(buffer.mem, expect, 1, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_unpack_set_timestamp)
{
	CTR_BUF_DEFINE(buffer, 512);

	char *input = "8100000000648d56b9";

	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);

	int64_t ts;

	int ret = ctr_cloud_msg_unpack_set_timestamp(&buffer, &ts);
	zassert_ok(ret, "ctr_cloud_msg_unpack_set_timestamp failed");

	zassert_equal(ts, (int64_t)1686984377, "ts");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_stats)
{
	CTR_BUF_DEFINE(buffer, 512);

	int ret = ctr_cloud_msg_pack_stats(&buffer);
	zassert_ok(ret, "ctr_cloud_msg_pack_stats failed");

	char *expectHex = "05bf0000010b020c030d040e050f0610071108120913ff";

	CTR_BUF_DEFINE(expect, 512);

	PRINT_CTR_BUF(buffer);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");
	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_config)
{
	CTR_BUF_DEFINE(buffer, 512);

	int ret = ctr_cloud_msg_pack_config(&buffer);
	zassert_ok(ret, "ctr_cloud_msg_pack_config failed");

	char *expectHex =
		"0273a9151b127641bb009f781c61707020636f6e66696720696e74657276616c2d73616d706c652035"
		"781d61707020636f6e66696720696e74657276616c2d7265706f7274203630782861707020636f6e66"
		"6967206261636b75702d7265706f72742d636f6e6e65637465642066616c7365ff";

	CTR_BUF_DEFINE(expect, 512);

	// PRINT_CTR_BUF(buffer);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");
	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");

	uint64_t hash_expect = 0x73a9151b127641bb;
	uint64_t hash;
	ctr_cloud_msg_get_hash(&buffer, &hash);

	zassert_equal(hash, hash_expect, "hash equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_unpack_download_config)
{
	CTR_BUF_DEFINE(buffer, 512);

	char *input = "820083781d61707020636f6e66696720696e74657276616c2d73616d706c65203130781d6170"
		      "7020636f6e66696720696e74657276616c2d7265706f7274203630782761707020636f6e6669"
		      "67206261636b75702d7265706f72742d636f6e6e65637465642074727565";

	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);

	struct ctr_cloud_msg_dlconfig config;

	int ret = ctr_cloud_msg_unpack_config(&buffer, &config);
	zassert_ok(ret, "ctr_cloud_msg_unpack_config failed");

	zassert_equal(config.lines, 3, "lines");

	CTR_BUF_DEFINE(line, CONFIG_SHELL_CMD_BUFF_SIZE + 1);

	ctr_buf_reset(&line);
	ret = ctr_cloud_msg_dlconfig_get_next_line(&config, &line);
	zassert_ok(ret, "ctr_cloud_msg_dlconfig_get_next_line failed");

	zassert_true(strcmp(line.mem, "app config interval-sample 10") == 0, "line 1");

	ctr_buf_reset(&line);
	ret = ctr_cloud_msg_dlconfig_get_next_line(&config, &line);
	zassert_ok(ret, "ctr_cloud_msg_dlconfig_get_next_line failed");

	zassert_true(strcmp(line.mem, "app config interval-report 60") == 0, "line 2");

	ctr_buf_reset(&line);
	ret = ctr_cloud_msg_dlconfig_get_next_line(&config, &line);
	zassert_ok(ret, "ctr_cloud_msg_dlconfig_get_next_line failed");

	zassert_true(strcmp(line.mem, "app config backup-report-connected true") == 0, "line 3");

	ret = ctr_cloud_msg_dlconfig_get_next_line(&config, &line);
	zassert_equal(ret, -ENODATA, "ctr_cloud_msg_dlconfig_get_next_line failed");
}

ZTEST(subsus_ctr_cloud_1_msg, test_unpack_download_shell)
{
	CTR_BUF_DEFINE(buffer, 512);

	char *input = "87a200826f61707020636f6e6669672073686f77781a61707020636f6e66696720696e746572"
		      "76616c2d73616d706c650150018dd0f29d0175f2abaf092b8d2f9f2b";

	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);

	struct ctr_cloud_msg_dlshell dlshell;

	int ret = ctr_cloud_msg_unpack_dlshell(&buffer, &dlshell);

	printf("ret: %d\n", ret);

	zassert_ok(ret, "ctr_cloud_msg_unpack_dlshell failed");
	zassert_equal(dlshell.commands, 2, "cmds len");

	char uuid[37];

	ret = ctr_cloud_util_uuid_to_str(dlshell.message_id, uuid, sizeof(uuid));

	printf("uuid: %s\n", uuid);

	ctr_cloud_uuid_t message_id = {0x01, 0x8d, 0xd0, 0xf2, 0x9d, 0x01, 0x75, 0xf2,
				       0xab, 0xaf, 0x09, 0x2b, 0x8d, 0x2f, 0x9f, 0x2b};
	zassert_mem_equal(dlshell.message_id, message_id, sizeof(ctr_cloud_uuid_t), "uuid equal");

	CTR_BUF_DEFINE(command, CONFIG_SHELL_CMD_BUFF_SIZE + 1);

	ctr_buf_reset(&command);
	ret = ctr_cloud_msg_dlshell_get_next_command(&dlshell, &command);
	zassert_ok(ret, "ctr_cloud_msg_dlshell_get_next_command failed");
	zassert_true(strcmp(command.mem, "app config show") == 0, "command 1");

	ctr_buf_reset(&command);
	ret = ctr_cloud_msg_dlshell_get_next_command(&dlshell, &command);
	zassert_ok(ret, "ctr_cloud_msg_dlshell_get_next_command failed");
	zassert_true(strcmp(command.mem, "app config interval-sample") == 0, "command 2");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_upload_shell)
{
	CTR_BUF_DEFINE(buffer, 512);

	ctr_cloud_uuid_t message_id = {0x01, 0x8d, 0xd0, 0xf2, 0x9d, 0x01, 0x75, 0xf2,
				       0xab, 0xaf, 0x09, 0x2b, 0x8d, 0x2f, 0x9f, 0x2b};

	int ret;
	struct ctr_cloud_msg_upshell upshell;

	ret = ctr_cloud_msg_pack_upshell_start(&upshell, &buffer, message_id);
	zassert_ok(ret, "ctr_cloud_msg_pack_upshell failed");

	ret = ctr_cloud_msg_pack_upshell_add_response(&upshell, "help", 0,
						      "\r\nout help\r\napp\r\nconfig\r\n");
	zassert_ok(ret, "ctr_cloud_msg_pack_upshell failed");

	ret = ctr_cloud_msg_pack_upshell_add_response(&upshell, "app", -22, "\r\nerror\r\n");
	zassert_ok(ret, "ctr_cloud_msg_pack_upshell failed");

	ret = ctr_cloud_msg_pack_upshell_end(&upshell);
	zassert_ok(ret, "ctr_cloud_msg_pack_upshell failed");

	// PRINT_CTR_BUF(buffer);

	char *expectHex =
		"07bf0150018dd0f29d0175f2abaf092b8d2f9f2b009fbf006468656c70029f686f75742068656c7063"
		"61707066636f6e666967ffffbf00636170700135029f656572726f72ffffffff";

	CTR_BUF_DEFINE(expect, 512);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");
	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_upload_firmware_app_download)
{
	CTR_BUF_DEFINE(buffer, 512);

	const char *frimware = "firmware:v1.0.0";

	struct ctr_cloud_upfirmware upfirmware = {
		.target = "app", .type = "download", .max_length = 1024, .firmware = frimware};

	int ret = ctr_cloud_msg_pack_firmware(&buffer, &upfirmware);

	zassert_ok(ret, "ctr_cloud_msg_pack_firmware failed");

	PRINT_CTR_BUF(buffer);

	char *expectHex =
		"08bf00636170700168646f776e6c6f616404190400056f6669726d776172653a76312e302e30ff";

	CTR_BUF_DEFINE(expect, 512);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");

	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_upload_firmware_app_next)
{
	CTR_BUF_DEFINE(buffer, 512);

	// a8a89f50-8a08-4b22-9964-1bf4a50f65ed
	ctr_cloud_uuid_t id = {0xa8, 0xa8, 0x9f, 0x50, 0x8a, 0x08, 0x4b, 0x22,
			       0x99, 0x64, 0x1b, 0xf4, 0xa5, 0x0f, 0x65, 0xed};

	struct ctr_cloud_upfirmware upfirmware = {
		.target = "app", .type = "next", .id = id, .offset = 100, .max_length = 1024};

	int ret = ctr_cloud_msg_pack_firmware(&buffer, &upfirmware);

	zassert_ok(ret, "ctr_cloud_msg_pack_firmware failed");

	PRINT_CTR_BUF(buffer);

	char *expectHex =
		"08bf006361707001646e6578740250a8a89f508a084b2299641bf4a50f65ed03186404190400ff";

	CTR_BUF_DEFINE(expect, 512);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");

	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_pack_upload_firmware_app_confirmed)
{
	CTR_BUF_DEFINE(buffer, 512);

	int ret = settings_subsys_init();
	zassert_ok(ret, "Expected settings to init");

	// a8a89f50-8a08-4b22-9964-1bf4a50f65ed
	ctr_cloud_uuid_t id = {0xa8, 0xa8, 0x9f, 0x50, 0x8a, 0x08, 0x4b, 0x22,
			       0x99, 0x64, 0x1b, 0xf4, 0xa5, 0x0f, 0x65, 0xed};

	ret = ctr_cloud_util_save_firmware_update_id(id);
	printf("\nctr_cloud_util_save_firmware_update_id ret: %d\n", ret);
	zassert_ok(ret, "ctr_cloud_util_save_firmware_update_id failed");

	ctr_cloud_uuid_t fuid;
	ret = ctr_cloud_util_get_firmware_update_id(fuid);
	zassert_ok(ret, "ctr_cloud_util_get_firmware_update_id failed");

	struct ctr_cloud_upfirmware upfirmware = {
		.target = "app",
		.type = "confirmed",
		.id = fuid,
	};

	ret = ctr_cloud_msg_pack_firmware(&buffer, &upfirmware);
	zassert_ok(ret, "ctr_cloud_msg_pack_firmware failed");

	PRINT_CTR_BUF(buffer);

	char *expectHex =
		"08bf00636170700169636f6e6669726d65640250a8a89f508a084b2299641bf4a50f65edff";

	CTR_BUF_DEFINE(expect, 512);

	expect.len = hex2bin(expectHex, strlen(expectHex), expect.mem, expect.size);

	zassert_equal(buffer.len, expect.len, "len equal");
	zassert_mem_equal(buffer.mem, expect.mem, expect.len, "mem equal");
}

ZTEST(subsus_ctr_cloud_1_msg, test_unpack_download_firmware)
{
	CTR_BUF_DEFINE(buffer, 512);

	char *input = "88a60063617070016566697273740250a8a89f508a084b2299641bf4a50f65ed04181e05581e"
		      "aabbccddee00112233445566778899aabbccddee0011223344556677889906181e";

	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);

	struct ctr_cloud_msg_dlfirmware dlfirmware;

	int ret = ctr_cloud_msg_unpack_dlfirmware(&buffer, &dlfirmware);
	zassert_ok(ret, "ctr_cloud_msg_unpack_dlfirmware failed");

	// a8a89f50-8a08-4b22-9964-1bf4a50f65ed
	ctr_cloud_uuid_t id = {0xa8, 0xa8, 0x9f, 0x50, 0x8a, 0x08, 0x4b, 0x22,
			       0x99, 0x64, 0x1b, 0xf4, 0xa5, 0x0f, 0x65, 0xed};
	zassert_mem_equal(dlfirmware.id, id, sizeof(ctr_cloud_uuid_t), "id equal");

	zassert_true(strcmp(dlfirmware.target, "app") == 0, "target");
	zassert_true(strcmp(dlfirmware.type, "first") == 0, "type");

	zassert_equal(dlfirmware.offset, 0, "offset");
	zassert_equal(dlfirmware.length, 30, "length");

	zassert_equal(dlfirmware.firmware_size, 30, "firmware_size");

	// data aabbccddee00112233445566778899aabbccddee00112233445566778899
	uint8_t data[30] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x00, 0x11, 0x22, 0x33, 0x44,
			    0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee,
			    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
	zassert_mem_equal(dlfirmware.data, data, 30, "data equal");
}

ZTEST_SUITE(subsus_ctr_cloud_1_msg, NULL, mock_global_setup_suite, NULL, NULL, NULL);
