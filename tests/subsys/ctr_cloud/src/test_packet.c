/** @file
 *  @brief cloud packet test suite
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <chester/ctr_buf.h>
#include <ctr_cloud_packet.h>

/* Test unpack packet */
ZTEST(subsus_ctr_cloud_0_packet, test_unpack_a)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "60d05fb58c7c550b80b0000140047273696f6e6676312e342e30";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_unpack failed");
	zassert_equal(pck.serial_number, 2159017985, "serial_number not equal");
	zassert_equal(pck.sequence, 4, "sequence not equal");
	zassert_equal(pck.flags, CTR_CLOUD_PACKET_FLAG_LAST, "flags not equal");
	zassert_equal(pck.data_len, 12, "data_len not equal");
	zassert_equal_ptr(pck.data, buffer.mem + CTR_CLOUD_PACKET_MIN_SIZE, "data not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_unpack_b)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "478acfb86b963d2b80b000012014";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_unpack failed");
	zassert_equal(pck.serial_number, 2159017985, "serial_number not equal");
	zassert_equal(pck.sequence, 20, "sequence not equal");
	zassert_equal(pck.flags, CTR_CLOUD_PACKET_FLAG_ACK, "flags not equal");
	zassert_equal(pck.data_len, 0, "data_len not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_unpack_c)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "ea47c658ff37f77680b000010002ffd6987da1";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_unpack failed");
	zassert_equal(pck.serial_number, 2159017985, "serial_number not equal");
	zassert_equal(pck.sequence, 2, "sequence not equal");
	zassert_equal(pck.flags, 0, "flags not equal");
	zassert_equal(pck.data_len, 5, "data_len not equal");
	zassert_equal_ptr(pck.data, buffer.mem + CTR_CLOUD_PACKET_MIN_SIZE, "data not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_unpack_d)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "a6bce7c20f32fbcd80b00001c00381000001886ccd11e5";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_unpack failed");
	zassert_equal(pck.serial_number, 2159017985, "serial_number not equal");
	zassert_equal(pck.sequence, 3, "sequence not equal");
	zassert_equal(pck.flags, CTR_CLOUD_PACKET_FLAG_FIRST | CTR_CLOUD_PACKET_FLAG_LAST,
		      "flags not equal");
	zassert_equal(pck.data_len, 9, "data_len not equal");
	zassert_equal_ptr(pck.data, buffer.mem + CTR_CLOUD_PACKET_MIN_SIZE, "data not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_unpack_e)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "49247ac38dacbbf280b000013001";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_unpack failed");
	zassert_equal(pck.serial_number, 2159017985, "serial_number not equal");
	zassert_equal(pck.sequence, 1, "sequence not equal");
	zassert_equal(pck.flags, CTR_CLOUD_PACKET_FLAG_ACK | CTR_CLOUD_PACKET_FLAG_POLL,
		      "flags not equal");
	zassert_equal(pck.data_len, 0, "data_len not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_unpack_error_short)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "49247ac38dacbbf280b000";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_equal(ret, -74, "ctr_cloud_packet_unpack failed");
}

ZTEST(subsus_ctr_cloud_0_packet, test_unpack_error_bad_hash)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);

	char *input = "00247ac38dacbbf280b000013001";
	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	buffer.len = hex2bin(input, strlen(input), buffer.mem, buffer.size);
	struct ctr_cloud_packet pck;

	int ret = ctr_cloud_packet_unpack(&pck, claim_token, &buffer);

	zassert_equal(ret, -74, "ctr_cloud_packet_unpack failed");
}

ZTEST(subsus_ctr_cloud_0_packet, test_pack_a)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);
	ctr_buf_fill(&buffer, 0xff);

	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	struct ctr_cloud_packet pck;

	pck.serial_number = 2159017985;
	pck.sequence = 20;
	pck.flags = CTR_CLOUD_PACKET_FLAG_ACK;
	pck.data_len = 0;

	int ret = ctr_cloud_packet_pack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_pack failed");
	zassert_equal(buffer.len, 14, "buffer.len not equal");

	char *expected = "478acfb86b963d2b80b000012014";

	uint8_t expectedBin[14];
	hex2bin(expected, strlen(expected), expectedBin, sizeof(expectedBin));

	zassert_equal(memcmp(buffer.mem, expectedBin, buffer.len), 0, "buffer.mem not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_pack_b)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);
	ctr_buf_fill(&buffer, 0xff);

	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	uint8_t data[1] = {0x00};
	struct ctr_cloud_packet pck;

	pck.serial_number = 2159017985;
	pck.sequence = 0;
	pck.flags = CTR_CLOUD_PACKET_FLAG_FIRST | CTR_CLOUD_PACKET_FLAG_LAST;
	pck.data_len = 1;
	pck.data = data;

	int ret = ctr_cloud_packet_pack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_pack failed");
	zassert_equal(buffer.len, 15, "buffer.len not equal");

	char *expected = "3cd56e3ca930323680b00001c00000";

	uint8_t expectedBin[15];
	hex2bin(expected, strlen(expected), expectedBin, sizeof(expectedBin));

	zassert_equal(memcmp(buffer.mem, expectedBin, buffer.len), 0, "buffer.mem not equal");
}

ZTEST(subsus_ctr_cloud_0_packet, test_pack_c)
{
	CTR_BUF_DEFINE(buffer, CTR_CLOUD_PACKET_MAX_SIZE);
	ctr_buf_fill(&buffer, 0xff);

	uint8_t claim_token[16] = {0x98, 0xa8, 0x85, 0x6b, 0xa6, 0x53, 0x4b, 0xd5,
				   0x21, 0x21, 0x76, 0xb2, 0x2f, 0x3a, 0xcb, 0xb3};
	uint8_t data[] = {0x06, 0x43, 0xc8, 0xb2, 0x39};
	struct ctr_cloud_packet pck;

	pck.serial_number = 2159017985;
	pck.sequence = 0;
	pck.flags = CTR_CLOUD_PACKET_FLAG_FIRST;
	pck.data_len = 5;
	pck.data = data;

	int ret = ctr_cloud_packet_pack(&pck, claim_token, &buffer);

	zassert_ok(ret, "ctr_cloud_packet_pack failed");
	zassert_equal(buffer.len, 19, "buffer.len not equal");

	char *expected = "66df44241b89312280b0000180000643c8b239";

	uint8_t expectedBin[19];
	hex2bin(expected, strlen(expected), expectedBin, sizeof(expectedBin));

	zassert_equal(memcmp(buffer.mem, expectedBin, buffer.len), 0, "buffer.mem not equal");
}

ZTEST_SUITE(subsus_ctr_cloud_0_packet, NULL, NULL, NULL, NULL, NULL);
