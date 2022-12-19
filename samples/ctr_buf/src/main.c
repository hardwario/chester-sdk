/* CHESTER includes */
#include <chester/ctr_buf.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Standard includes */
#include <stdint.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);

	CTR_BUF_DEFINE_STATIC(buf, 16);

	const uint8_t mem[] = {0x01, 0x02, 0x03, 0x04};
	ctr_buf_reset(&buf);
	ctr_buf_append_mem(&buf, mem, sizeof(mem));
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: mem");

	const uint8_t *str = "12345678";
	ctr_buf_reset(&buf);
	ctr_buf_append_str(&buf, str);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: str");

	ctr_buf_reset(&buf);
	ctr_buf_append_char(&buf, 0x01);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: char");

	ctr_buf_reset(&buf);
	ctr_buf_append_s8(&buf, 0x01);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: s8");

	ctr_buf_reset(&buf);
	ctr_buf_append_s16(&buf, 0x0102);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: s16");

	ctr_buf_reset(&buf);
	ctr_buf_append_s32(&buf, 0x01020304);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: s32");

	ctr_buf_reset(&buf);
	ctr_buf_append_s64(&buf, 0x0102030405060708);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: s64");

	ctr_buf_reset(&buf);
	ctr_buf_append_u8(&buf, 0x01);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: u8");

	ctr_buf_reset(&buf);
	ctr_buf_append_u16(&buf, 0x0102);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: u16");

	ctr_buf_reset(&buf);
	ctr_buf_append_u32(&buf, 0x01020304);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: u32");

	ctr_buf_reset(&buf);
	ctr_buf_append_u64(&buf, 0x0102030405060708);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: u64");

	ctr_buf_reset(&buf);
	ctr_buf_append_float(&buf, 1.234f);
	LOG_HEXDUMP_INF(ctr_buf_get_mem(&buf), ctr_buf_get_used(&buf), "Test: float");
}
