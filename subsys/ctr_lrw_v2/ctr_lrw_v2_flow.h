#ifndef CHESTER_SUBSYS_CTR_LRW_V2_FLOW_H_
#define CHESTER_SUBSYS_CTR_LRW_V2_FLOW_H_

/* CHESTER includes */
#include <chester/ctr_lrw_v2.h>

/* Zephyr includes */
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

/* Standard includes */
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int ctr_lrw_v2_flow_set_recv_cb(ctr_lrw_v2_recv_cb callback);
int ctr_lrw_v2_flow_reset(void);
int ctr_lrw_v2_flow_start(void);
int ctr_lrw_v2_flow_setup(void);
int ctr_lrw_v2_flow_join(int retries, k_timeout_t timeout);
int ctr_lrw_v2_flow_send(int retries, k_timeout_t timeout, const struct send_msgq_data *data);
int ctr_lrw_v2_flow_poll(void);
int ctr_lrw_v2_flow_cmd_test_uart(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_flow_cmd_test_reset(const struct shell *shell, size_t argc, char **argv);
int ctr_lrw_v2_flow_cmd_test_cmd(const struct shell *shell, size_t argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* CHESTER_SUBSYS_CTR_LRW_V2_FLOW_H_ */
