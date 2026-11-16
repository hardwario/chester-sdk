/*
 * Copyright (c) 2026 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#ifndef TESTS_SUBSYS_CTR_LTE_V2_SRC_STACK_H_
#define TESTS_SUBSYS_CTR_LTE_V2_SRC_STACK_H_

#include "mock.h"

#include <chester/ctr_lte_v2.h>

/* Zephyr includes */
#include <zephyr/ztest.h>

/* Standard includes */
#include <string.h>

/* clang-format off */
#define URC(ms, urc) { .rx=urc, .delay_ms=ms }
#define RX_DATA(ms, urc, data) { .rx=urc, .rx2=data, .delay_ms=ms }

#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_OVERRIDES_CONFIG)
#define ITEM_BANDLOCK {"AT%XBANDLOCK=1,\"0000000000000000000000100000000000000000000000000000000000001011000011110001100010011010\"", "OK"}
#define ITEMS_NETWORK {"AT+COPS=1,2,\"23003\"", "OK"}, {"AT+CGDCONT=0,\"IP\",\"hardwario\"", "OK"}
#define ITEM_CONNECT {"AT#XCONNECT=\"46.101.214.168\",5003"}
#else
#define ITEM_BANDLOCK {"AT%XBANDLOCK=0", "OK"}
#define ITEMS_NETWORK {"AT+COPS=0", "OK"}, {"AT+CGDCONT=0,\"IP\"", "OK"}
#define ITEM_CONNECT {"AT#XCONNECT=\"192.168.192.4\",5003"}
#endif

#if IS_ENABLED(CONFIG_TEST_FEATURE_STACK_FPLMN_ERASE_FAILS)
/* Forbidden network on the SIM, which refuses the erase: attach anyway */
#define ITEMS_FPLMN \
	{"AT+CRSM=176,28539,0,0,12", "+CRSM: 144,0,\"32F030FFFFFFFFFFFFFFFFFF\"", "OK"}, \
	{"AT+CRSM=214,28539,0,0,12,\"FFFFFFFFFFFFFFFFFFFFFFFF\"", "ERROR"}
#else
#define ITEMS_FPLMN \
	{"AT+CRSM=176,28539,0,0,12", "+CRSM: 144,0,\"FFFFFFFFFFFFFFFFFFFFFFFF\"", "OK"}
#endif

/* From the modem boot to the SIM read, after which the FSM is in the attach state */
#define ITEMS_PREPARE \
	{"AT", "OK"}, \
	{"AT+CGSN=1", "+CGSN: \"351358815178345\"", "OK"}, \
	{"AT%HWVERSION", "%HWVERSION: nRF9160 SICA B1A", "OK"}, \
	{"AT%SHORTSWVER", "%SHORTSWVER: nrf9160_1.3.2", "OK"}, \
	{"AT#XSLMVER", "#XSLMVER: \"2.5.2\",\"2.5.0-lte-5ccd2d4dd54c\"", "OK"}, \
	{"AT#XVERSION", "#XVERSION: \"v1.7.0\"", "OK"}, \
	{"AT+CFUN=0", "OK"}, \
	{"AT%XPOFWARN=1,30", "OK"}, \
	{"AT%XTEMPHIGHLVL=70", "OK"}, \
	{"AT%XTEMP=1", "OK"}, \
	{"AT%XSYSTEMMODE=1,1,1,1", "OK"}, \
	{"AT%XDATAPRFL=0", "OK"}, \
	ITEM_BANDLOCK, \
	{"AT%XSIM=1", "OK"}, \
	{"AT%XNETTIME=1", "OK"}, \
	{"AT%MDMEV=1", "OK"}, \
	{"AT%REL14FEAT=1,1,1,1,0", "OK"}, \
	{"AT%RAI=1", "OK"}, \
	{"AT+CPSMS=1,,,\"00111000\",\"00000000\"", "OK"}, \
	{"AT+CEPPI=1", "OK"}, \
	{"AT+CEREG=5", "OK"}, \
	{"AT+CGEREP=1", "OK"}, \
	{"AT+CMEE=1", "OK"}, \
	{"AT+CNEC=24", "OK"}, \
	{"AT%XCOEX0=1,1,1565,1586", "OK"}, \
	{"AT+CSCON=1", "OK"}, \
	ITEMS_NETWORK, \
	{"AT+CGAUTH=0,0", "OK"}, \
	{"AT%XMODEMSLEEP=1,500,10240", "OK"}, \
	{"AT+CFUN=1", "%XMODEMSLEEP: 4", "OK"}, \
	URC(0, "%XMODEMSLEEP: 4,0"), \
	URC(0, "+CEREG: 0"), \
	URC(500, "+CEREG: 2,\"B4DC\",\"000AE520\",9"), \
	URC(100, "%XSIM: 1"), \
	{"AT+CIMI", "901288910017273", "OK"}, \
	{"AT+CIMI", "901288910017273", "OK"}, \
	{"AT%XICCID", "%XICCID: 89882390000262826558", "OK"}, \
	ITEMS_FPLMN

/* From the registration to the open socket, after which the FSM is in the ready state */
#define ITEMS_REGISTER \
	URC(100, "+CSCON: 1"), \
	URC(50, "+CEREG: 5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\""), \
	{"AT+COPS?", "+COPS: 1,2,\"23003\",9", "OK"}, \
	{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"", "OK"}, \
	{"AT%XCBAND", "%XCBAND: 20", "OK"}, \
	{"AT+CEINFO?", "+CEINFO: 0,1,C,8,1,-79,21", "OK"}, \
	{"AT+CGATT?", "+CGATT: 1", "OK"}, \
	{"AT+CGACT?", "+CGACT: 0,1", "OK"}, \
	{"AT+CGDCONT?", "+CGDCONT: 0,\"IP\",\"hardwario\",\"172.28.0.139\",0,0", "OK"}, \
	{"AT#XSOCKET=1,2,0,0", "#XSOCKET: 0,2,17", "OK"}, \
	ITEM_CONNECT, \
	/* Registered, the socket connects a while later */ \
	URC(1000, "#XCONNECT: 1"), \
	URC(0, "OK"), \
	ITEM_CONEVAL

/* Failed attach: the retry delay state switches the modem off */
#define ITEMS_RETRY_DELAY \
	{"AT+CFUN=4", "OK"}, \
	{"AT#XSLEEP=2", "OK"}

/* The check before a send from the ready state, with the socket the modem reports */
#define ITEMS_FLOW_CHECK_XSOCKET(xsocket) \
	{"AT+CFUN?", "+CFUN: 1", "OK"}, \
	{"AT+CEREG?", "+CEREG: 5,5,\"B4DC\",\"000AE520\",9,0,18,\"00000000\",\"00111000\"", "OK"}, \
	{"AT+CGATT?", "+CGATT: 1", "OK"}, \
	{"AT+CGACT?", "+CGACT: 0,1", "OK"}, \
	{"AT#XSOCKET?", xsocket, "OK"}

/* <handle>,<family>,<role>,<type>,<cid>: a datagram client socket */
#define ITEMS_FLOW_CHECK ITEMS_FLOW_CHECK_XSOCKET("#XSOCKET: 0,1,0,2,0")

#define ITEM_CONEVAL \
	{"AT%CONEVAL", "%CONEVAL: 0,1,7,95,30,49,\"000AE520\",\"23003\",135,6447,20,0,0,-8,2,1,73", "OK"}

/* The network releases the RRC connection ms after the previous item, the FSM goes to sleep */
#define ITEMS_SLEEP(ms) \
	URC(ms, "+CSCON: 0"), \
	URC(0, "%XMODEMSLEEP: 1,89999999"), \
	{"AT#XSLEEP=2", "OK"}
/* clang-format on */

#define ASSERT_FSM_STATE(expected)                                                                 \
	zassert_equal(strcmp(ctr_lte_v2_get_state(), expected), 0,                                 \
		      "expected state " expected " but got: %s", ctr_lte_v2_get_state())

/* Enable the stack and run the attach script; the FSM ends in the ready state */
void stack_attach(void);

/* Poll until the FSM is in the state, fail after the timeout */
void stack_wait_for_state(const char *state, k_timeout_t timeout);

#endif
