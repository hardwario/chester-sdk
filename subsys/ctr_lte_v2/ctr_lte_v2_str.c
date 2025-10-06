/*
 * Copyright (c) 2025 HARDWARIO a.s.
 *
 * SPDX-License-Identifier: LicenseRef-HARDWARIO-5-Clause
 */

#include "ctr_lte_v2_str.h"

/* CHESTER includes */
#include <chester/ctr_lte_v2.h>

/* Standard includes */
#include <stddef.h>
#include <stdint.h>

const char *INVALID = "invalid";

const char *ctr_lte_v2_str_coneval_result(int result)
{
	switch (result) {
	case 0:
		return "Connection pre-evaluation successful";
	case 1:
		return "Evaluation failed, no cell available";
	case 2:
		return "Evaluation failed, UICC not available";
	case 3:
		return "Evaluation failed, only barred cells available";
	case 4:
		return "Evaluation failed, busy";
	case 5:
		return "Evaluation failed, aborted because of higher priority operation";
	case 6:
		return "Evaluation failed, not registered";
	case 7:
		return "Evaluation failed, unspecified";
	default:
		return "Evaluation failed, unknown result";
	}
}

const char *ctr_lte_v2_str_cereg_stat(enum ctr_lte_v2_cereg_param_stat stat)
{
	switch (stat) {
	case CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED:
		return "not-registered";
	case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME:
		return "registered-home";
	case CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING:
		return "searching";
	case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTRATION_DENIED:
		return "registration-denied";
	case CTR_LTE_V2_CEREG_PARAM_STAT_UNKNOWN:
		return "unknown";
	case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING:
		return "registered-roaming";
	case CTR_LTE_V2_CEREG_PARAM_STAT_SIM_FAILURE:
		return "sim-failure";
	default:
		return INVALID;
	}
}

const char *ctr_lte_v2_str_cereg_stat_human(enum ctr_lte_v2_cereg_param_stat stat)
{
	switch (stat) {
	case CTR_LTE_V2_CEREG_PARAM_STAT_NOT_REGISTERED:
		return "Not registered (idle)";
	case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_HOME:
		return "Registered (home network)";
	case CTR_LTE_V2_CEREG_PARAM_STAT_SEARCHING:
		return "Searching for network";
	case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTRATION_DENIED:
		return "Registration denied";
	case CTR_LTE_V2_CEREG_PARAM_STAT_UNKNOWN:
		return "Unknown (e.g. out of E-UTRAN coverage)";
	case CTR_LTE_V2_CEREG_PARAM_STAT_REGISTERED_ROAMING:
		return "Registered (roaming)";
	case CTR_LTE_V2_CEREG_PARAM_STAT_SIM_FAILURE:
		return "SIM failure";
	default:
		return "Invalid/unsupported status";
	}
}

const char *ctr_lte_v2_str_act(enum ctr_lte_v2_cereg_param_act act)
{
	switch (act) {
	case CTR_LTE_V2_CEREG_PARAM_ACT_UNKNOWN:
		return "unknown";
	case CTR_LTE_V2_CEREG_PARAM_ACT_LTE:
		return "lte-m";
	case CTR_LTE_V2_CEREG_PARAM_ACT_NBIOT:
		return "nb-iot";
	default:
		return INVALID;
	}
}

const char *ctr_lte_v2_str_fsm_event(enum ctr_lte_v2_event event)
{
	switch (event) {
	case CTR_LTE_V2_EVENT_ERROR:
		return "error";
	case CTR_LTE_V2_EVENT_TIMEOUT:
		return "timeout";
	case CTR_LTE_V2_EVENT_ENABLE:
		return "enable";
	case CTR_LTE_V2_EVENT_READY:
		return "ready";
	case CTR_LTE_V2_EVENT_SIMDETECTED:
		return "simdetected";
	case CTR_LTE_V2_EVENT_REGISTERED:
		return "registered";
	case CTR_LTE_V2_EVENT_DEREGISTERED:
		return "deregistered";
	case CTR_LTE_V2_EVENT_RESET_LOOP:
		return "resetloop";
	case CTR_LTE_V2_EVENT_SOCKET_OPENED:
		return "socket_opened";
	case CTR_LTE_V2_EVENT_XMODMSLEEEP:
		return "xmodmsleep";
	case CTR_LTE_V2_EVENT_CSCON_0:
		return "cscon_0";
	case CTR_LTE_V2_EVENT_CSCON_1:
		return "cscon_1";
	case CTR_LTE_V2_EVENT_XTIME:
		return "xtime";
	case CTR_LTE_V2_EVENT_SEND:
		return "send";
	case CTR_LTE_V2_EVENT_RECV:
		return "recv";
	case CTR_LTE_V2_EVENT_XGPS_ENABLE:
		return "xgps_enable";
	case CTR_LTE_V2_EVENT_XGPS_DISABLE:
		return "xgps_disable";
	case CTR_LTE_V2_EVENT_XGPS:
		return "xgps";
	}
	return INVALID;
}
