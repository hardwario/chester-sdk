
#include "mock.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <chester/ctr_buf.h>
#include <chester/ctr_lte_v2.h>
#include <ctr_cloud_packet.h>

static bool is_started = false;

int ctr_lte_v2_get_imei(uint64_t *imei)
{
	*imei = 987654321098761;
	return 0;
}

int ctr_lte_v2_get_imsi(uint64_t *imsi)
{
	*imsi = 123456789012341;
	return 0;
}

int ctr_lte_v2_get_modem_fw_version(char **version)
{
	*version = "v1.5.0";
	return 0;
}

int ctr_lte_v2_start(void)
{
	is_started = true;
	return 0;
}

int ctr_lte_v2_stop(void)
{
	is_started = false;
	return 0;
}

int ctr_lte_v2_wait_on_modem_sleep(k_timeout_t delay)
{
	return 0;
}

int ctr_lte_v2_prepare(void)
{
	return 0;
}

int ctr_lte_v2_attach(void)
{
	return 0;
}

int ctr_lte_v2_detach(void)
{
	return 0;
}

void print_bufer(uint8_t *buf, size_t len)
{
	for (int i = 0; i < len; i++) {
		printf("%02x", buf[i]);
	}
	printf("\n");
}

static struct {
	int index;
	int size;
	char **list;

	uint8_t recv[CTR_CLOUD_PACKET_MAX_SIZE];
} mock_com;

void mock_ctr_lte_v2_set_send_recv(char **list, int size)
{
	mock_com.index = 0;
	mock_com.size = size;
	mock_com.list = list;
}

int ctr_lte_v2_send_recv(const struct ctr_lte_v2_send_recv_param *param, bool rai)
{
	printf("mock: ctr_lte_v2_send_recv %d\n", mock_com.index);

	for (size_t i = 0; i < param->send_len; i++) {
		printf("%02x", ((uint8_t *)param->send_buf)[i]);
	}
	printf("\n");

	if (mock_com.index >= mock_com.size) {
		return -1;
	}

	char *recvStr = mock_com.list[mock_com.index++];

	hex2bin(recvStr, strlen(recvStr), mock_com.recv, sizeof(mock_com.recv));

	zassert_mem_equal(mock_com.recv, param->send_buf, param->send_len, NULL);

	char *sendStr = mock_com.list[mock_com.index++];
	*param->recv_len = 0;

	if (sendStr != NULL && param->recv_buf) {
		*param->recv_len =
			hex2bin(sendStr, strlen(sendStr), param->recv_buf, param->recv_size);
	}

	return 0;
}

int ctr_lte_v2_get_conn_param(struct ctr_lte_v2_conn_param *param)
{
	param->valid = true;
	param->eest = 11;
	param->ecl = 12;
	param->rsrp = 13;
	param->rsrq = 14;
	param->snr = 15;
	param->plmn = 16;
	param->cid = 17;
	param->band = 18;
	param->earfcn = 19;
	return 0;
}
