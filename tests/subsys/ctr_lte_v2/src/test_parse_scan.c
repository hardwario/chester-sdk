/** @file
 *  @brief ctr_lte_v2 parse test suite: neighbour cell measurement and network scan
 *
 *  Joins the 'parser' suite of test_parse.c.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <chester/ctr_lte_v2.h>
#include <ctr_lte_v2_parse.h>

#include "hw_lines.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define N2  SYN_NCELL SYN_NCELL
#define N5  N2 N2 SYN_NCELL
#define N10 N5 N5

/* The first cell of the line, 'nbr' neighbours follow it in the line */
#define CELL0(nbr) "0,\"074FEB50\",\"23002\",\"05F2\",31,941054,6300,488,67,17,1139290,1," nbr

/* A cell with no neighbours, the PCI makes it recognizable */
#define CELL_N(eci, pci) ",\"" eci "\",\"23002\",\"05F2\",65535,0,6300," pci ",65,12,1139290,0,0"

static struct ctr_lte_v2_ncellmeas_param param;

static void expect_cell(int i, uint32_t eci, const char *plmn, uint16_t tac, uint16_t adv,
			uint32_t earfcn, uint16_t pci, int16_t rsrp, int16_t rsrq, bool serving,
			uint8_t neighbor_count)
{
	const struct ctr_lte_v2_ncellmeas_cell *c = &param.cells[i];

	zassert_equal(c->eci, eci, "cells[%d].eci", i);
	zassert_str_equal(c->plmn, plmn, "cells[%d].plmn", i);
	zassert_equal(c->tac, tac, "cells[%d].tac", i);
	zassert_equal(c->adv, adv, "cells[%d].adv", i);
	zassert_equal(c->earfcn, earfcn, "cells[%d].earfcn", i);
	zassert_equal(c->pci, pci, "cells[%d].pci", i);
	zassert_equal(c->rsrp, rsrp, "cells[%d].rsrp", i);
	zassert_equal(c->rsrq, rsrq, "cells[%d].rsrq", i);
	zassert_equal(c->serving, serving, "cells[%d].serving", i);
	zassert_equal(c->neighbor_count, neighbor_count, "cells[%d].neighbor_count", i);
}

static void expect_ncell(int i, uint32_t earfcn, uint16_t pci, int16_t rsrp, int16_t rsrq,
			 int time_diff)
{
	const struct ctr_lte_v2_ncellmeas_ncell *n = &param.ncells[i];

	zassert_equal(n->earfcn, earfcn, "ncells[%d].earfcn", i);
	zassert_equal(n->pci, pci, "ncells[%d].pci", i);
	zassert_equal(n->rsrp, rsrp, "ncells[%d].rsrp", i);
	zassert_equal(n->rsrq, rsrq, "ncells[%d].rsrq", i);
	zassert_equal(n->time_diff, time_diff, "ncells[%d].time_diff", i);
}

static void expect_empty(void)
{
	zassert_false(param.valid, "valid");
	zassert_equal(param.num_cells, 0, "num_cells");
	zassert_equal(param.num_ncells, 0, "num_ncells");
	zassert_equal(param.cells[0].eci, 0, "cells[0] not cleared");
	zassert_equal(param.ncells[0].earfcn, 0, "ncells[0] not cleared");
}

/* -------- PLMN -------- */

ZTEST(parser, test_plmn_ok)
{
	zassert_ok(ctr_lte_v2_parse_plmn("23003"), "2-digit MNC");
	zassert_ok(ctr_lte_v2_parse_plmn("26295"), "2-digit MNC");
	zassert_ok(ctr_lte_v2_parse_plmn("310260"), "3-digit MNC");
	zassert_ok(ctr_lte_v2_parse_plmn("23007"), "MNC with a leading zero");
}

ZTEST(parser, test_plmn_invalid)
{
	zassert_equal(ctr_lte_v2_parse_plmn(NULL), -EINVAL, "NULL");
	zassert_equal(ctr_lte_v2_parse_plmn(""), -EBADMSG, "empty");
	zassert_equal(ctr_lte_v2_parse_plmn("1234"), -EBADMSG, "4 digits");
	zassert_equal(ctr_lte_v2_parse_plmn("2300"), -EBADMSG, "4 digits");
	zassert_equal(ctr_lte_v2_parse_plmn("2300123"), -EBADMSG, "7 digits");
	zassert_equal(ctr_lte_v2_parse_plmn("3102a0"), -EBADMSG, "non-digit");
	zassert_equal(ctr_lte_v2_parse_plmn("2300a"), -EBADMSG, "non-digit");
	zassert_equal(ctr_lte_v2_parse_plmn("00001"), -EPROTO, "MCC 000");
	zassert_equal(ctr_lte_v2_parse_plmn("000123"), -EPROTO, "MCC 000");
}

/* -------- %NCELLMEAS: captured lines -------- */

ZTEST(parser, test_ncellmeas_hw_registered)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_REGISTERED, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.status, 0, "status");
	zassert_equal(param.num_cells, 4, "num_cells");
	zassert_equal(param.num_ncells, 0, "num_ncells");
	expect_cell(0, 0x074FEB50, "23002", 0x05F2, 31, 6300, 488, 67, 17, true, 0);
	expect_cell(1, 0x074FEB51, "23002", 0x05F2, 65535, 6300, 339, 65, 12, false, 0);
	expect_cell(2, 0x074FEB0C, "23001", 0x3866, 65535, 6200, 14, 66, 14, false, 0);
	expect_cell(3, 0x000AE5CA, "23003", 0x8DCC, 65535, 3544, 135, 75, 32, false, 0);
}

ZTEST(parser, test_ncellmeas_hw_searching)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_SEARCHING, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.status, 0, "status");
	zassert_equal(param.num_cells, 1, "num_cells");
	zassert_equal(param.num_ncells, 1, "num_ncells");
	expect_cell(0, 0x074FEB50, "23002", 0x05F2, 17, 6300, 488, 68, 19, true, 1);
	zassert_equal(param.cells[0].ncell_first, 0, "ncell_first");
	expect_ncell(0, 6300, 339, 64, 12, 0);
}

ZTEST(parser, test_ncellmeas_hw_connected)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_CONNECTED, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 1, "num_cells");
	expect_cell(0, 0x074FEB50, "23002", 0x05F2, 15, 6300, 488, 67, 15, true, 1);
	expect_ncell(0, 6300, 339, 66, 14, 0);
}

ZTEST(parser, test_ncellmeas_hw_stopped)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_STOPPED, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.status, 0, "status");
	zassert_equal(param.num_cells, 3, "num_cells");
	expect_cell(2, 0x000AE5CA, "23003", 0x8DCC, 65535, 3544, 135, 75, 29, false, 0);
}

ZTEST(parser, test_ncellmeas_hw_nbiot_camped)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_NBIOT_CAMPED, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 1, "num_cells");
	expect_cell(0, 0x074FEBAC, "23001", 0x28B4, 0, 6247, 14, 73, 21, true, 0);
}

/* -------- %NCELLMEAS: lines ported from hio -------- */

ZTEST(parser, test_ncellmeas_one_cell_four_neighboring)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(
		"0,\"00011B07\",\"26295\",\"00B7\",10512,9034,2300,7,63,31,150344527,1,4,2300,"
		"8,60,29,92,2300,9,59,28,100,2400,10,56,27,162,2400,11,55,26,184",
		&param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 1, "num_cells");
	zassert_equal(param.num_ncells, 4, "num_ncells");
	expect_cell(0, 0x00011B07, "26295", 0x00B7, 10512, 2300, 7, 63, 31, true, 4);
	zassert_equal(param.cells[0].ncell_first, 0, "ncell_first");
	expect_ncell(0, 2300, 8, 60, 29, 92);
	expect_ncell(1, 2300, 9, 59, 28, 100);
	expect_ncell(2, 2400, 10, 56, 27, 162);
	expect_ncell(3, 2400, 11, 55, 26, 184);
}

ZTEST(parser, test_ncellmeas_two_cells_no_neighboring)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(
		"0,\"00011B07\",\"26295\",\"00B7\",10512,9034,2300,7,63,31,150344527,"
		"1,0,\"00011B08\",\"26295\",\"00B7\",65535,0,2300,9,62,30,150345527,0,0",
		&param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 2, "num_cells");
	zassert_equal(param.num_ncells, 0, "num_ncells");
	expect_cell(0, 0x00011B07, "26295", 0x00B7, 10512, 2300, 7, 63, 31, true, 0);
	expect_cell(1, 0x00011B08, "26295", 0x00B7, 65535, 2300, 9, 62, 30, false, 0);
}

ZTEST(parser, test_ncellmeas_complex)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(
		"0,\"000AE5CA\",\"23003\",\"8DCC\",65535,0,3544,135,67,31,549479,0,0,\"00011B07\","
		"\"26295\",\"00B7\",10512,9034,2300,7,63,31,150344527,1,3,2300,"
		"8,60,29,92,2300,9,59,28,100,2400,10,56,27,162,\"074FEB02\",\"23002\",\"05F2\","
		"65535,0,6300,226,60,9,549525,0,1,2400,11,55,26,184",
		&param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 3, "num_cells");
	zassert_equal(param.num_ncells, 4, "num_ncells");
	expect_cell(0, 0x000AE5CA, "23003", 0x8DCC, 65535, 3544, 135, 67, 31, false, 0);
	expect_cell(1, 0x00011B07, "26295", 0x00B7, 10512, 2300, 7, 63, 31, true, 3);
	expect_cell(2, 0x074FEB02, "23002", 0x05F2, 65535, 6300, 226, 60, 9, false, 1);
	zassert_equal(param.cells[1].ncell_first, 0, "cells[1].ncell_first");
	zassert_equal(param.cells[2].ncell_first, 3, "cells[2].ncell_first");
	expect_ncell(0, 2300, 8, 60, 29, 92);
	expect_ncell(2, 2400, 10, 56, 27, 162);
	expect_ncell(3, 2400, 11, 55, 26, 184);
}

#define HIO_COMPLEX_B                                                                              \
	"0,\"061ABD0C\",\"23001\",\"383E\",65535,0,6200,36,36,14,26023,1,0,"                       \
	"\"06235F0B\",\"23001\",\"383E\",65535,0,6200,452,36,13,26023,0,0,"                        \
	"\"06239B0C\",\"23001\",\"383E\",65535,0,6200,155,33,6,26023,0,0,"                         \
	"\"061ABD01\",\"23002\",\"05EA\",65535,0,6300,493,36,11,26059,0,0,"                        \
	"\"06235F00\",\"23002\",\"05EA\",65535,0,6300,303,33,6,26059,0,0,"                         \
	"\"000F6ECB\",\"23003\",\"8D04\",65535,0,3544,125,42,28,26068,0,0,"                        \
	"\"000F6ECA\",\"23003\",\"8D04\",65535,0,3544,124,36,15,26068,0,0"

ZTEST(parser, test_ncellmeas_complex_b)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HIO_COMPLEX_B, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, CTR_LTE_V2_NCELLMEAS_CELL_MAX, "num_cells");
	expect_cell(0, 0x061ABD0C, "23001", 0x383E, 65535, 6200, 36, 36, 14, true, 0);
	expect_cell(6, 0x000F6ECA, "23003", 0x8D04, 65535, 3544, 124, 36, 15, false, 0);
}

/* The modem found more than we keep: keep what fits, it is not an error */
ZTEST(parser, test_ncellmeas_cells_over_max)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(
		HIO_COMPLEX_B ",\"000F6ECC\",\"23003\",\"8D04\",65535,0,3544,126,30,9,26068,0,0",
		&param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, CTR_LTE_V2_NCELLMEAS_CELL_MAX, "num_cells");
	expect_cell(6, 0x000F6ECA, "23003", 0x8D04, 65535, 3544, 124, 36, 15, false, 0);
}

/* -------- %NCELLMEAS: status -------- */

ZTEST(parser, test_ncellmeas_status_1)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas("1", &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.status, 1, "status");
	zassert_equal(param.num_cells, 0, "num_cells");
}

ZTEST(parser, test_ncellmeas_status_2)
{
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas("2", &param), -EOPNOTSUPP, "status 2");
	expect_empty();
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas("4", &param), -EOPNOTSUPP, "status 4");
}

ZTEST(parser, test_ncellmeas_status_3_with_cells)
{
	char line[400];

	/* The status is the first character of the line */
	snprintf(line, sizeof(line), "3%s", HW_NCELLMEAS_REGISTERED + 1);
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(line, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.status, 3, "status");
	zassert_equal(param.num_cells, 4, "num_cells");
	expect_cell(0, 0x074FEB50, "23002", 0x05F2, 31, 6300, 488, 67, 17, true, 0);
	expect_cell(3, 0x000AE5CA, "23003", 0x8DCC, 65535, 3544, 135, 75, 32, false, 0);
}

ZTEST(parser, test_ncellmeas_status_3_no_cells)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas("3", &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.status, 3, "status");
	zassert_equal(param.num_cells, 0, "num_cells");
}

ZTEST(parser, test_ncellmeas_status_0_no_cells)
{
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas("0", &param), -EPROTO, "status 0 alone");
	expect_empty();
}

/* -------- %NCELLMEAS: the neighbour capacity -------- */

ZTEST(parser, test_ncellmeas_exactly_max_neighbors)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(CELL0("10") N10, &param),
		   "exactly the capacity is no error");

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 1, "num_cells");
	zassert_equal(param.num_ncells, CTR_LTE_V2_NCELLMEAS_NCELL_MAX, "num_ncells");
	zassert_equal(param.cells[0].neighbor_count, 10, "neighbor_count");
	expect_ncell(9, 6300, 339, 64, 12, 0);
}

ZTEST(parser, test_ncellmeas_over_max_neighbors)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(CELL0("11") N10 SYN_NCELL, &param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_ncells, CTR_LTE_V2_NCELLMEAS_NCELL_MAX, "num_ncells");
	zassert_equal(param.cells[0].neighbor_count, 10, "neighbor_count is what was stored");
}

/* The neighbours of the second cell do not fit: the count says how many were stored */
ZTEST(parser, test_ncellmeas_neighbors_split_over_cells)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(
		CELL0("6") N5 SYN_NCELL
		",\"074FEB51\",\"23002\",\"05F2\",65535,0,6300,339,65,12,1139290,0,6" N5 SYN_NCELL,
		&param));

	zassert_true(param.valid, "valid");
	zassert_equal(param.num_cells, 2, "num_cells");
	zassert_equal(param.num_ncells, 10, "num_ncells");
	zassert_equal(param.cells[0].neighbor_count, 6, "cells[0].neighbor_count");
	zassert_equal(param.cells[0].ncell_first, 0, "cells[0].ncell_first");
	zassert_equal(param.cells[1].neighbor_count, 4, "cells[1].neighbor_count is the stored");
	zassert_equal(param.cells[1].ncell_first, 6, "cells[1].ncell_first");
}

/* -------- %NCELLMEAS: invalid lines -------- */

ZTEST(parser, test_ncellmeas_eci_range)
{
	/* 0x0FFFFFFF (268435455) is the highest ECI, 0x10000000 is no cell */
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(CELL0("0") CELL_N("0FFFFFFF", "339"), &param),
		   "ECI max");
	zassert_equal(param.cells[1].eci, 0x0FFFFFFF, "ECI max");
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(CELL0("0") CELL_N("10000000", "339"), &param),
		      -ERANGE, "ECI over max");
	expect_empty();
}

ZTEST(parser, test_ncellmeas_invalid_fields)
{
	/* ECI with 7 digits */
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"74FEB50\",\"23002\",\"05F2\",31,941054,6300,488,67,17,1,1,0", &param),
		-EBADMSG, "ECI 7 digits");
	/* TAC with 3 digits */
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"074FEB50\",\"23002\",\"5F2\",31,941054,6300,488,67,17,1,1,0", &param),
		-EBADMSG, "TAC 3 digits");
	/* TAC not hex */
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"074FEB50\",\"23002\",\"05G2\",31,941054,6300,488,67,17,1,1,0", &param),
		-EBADMSG, "TAC not hex");
	/* PLMN: too short, MCC 000, not a digit */
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"074FEB50\",\"2300\",\"05F2\",31,941054,6300,488,67,17,1,1,0", &param),
		-EBADMSG, "PLMN 4 digits");
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"074FEB50\",\"00001\",\"05F2\",31,941054,6300,488,67,17,1,1,0", &param),
		-EPROTO, "MCC 000");
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"074FEB50\",\"2300a\",\"05F2\",31,941054,6300,488,67,17,1,1,0", &param),
		-EBADMSG, "PLMN not a digit");
	/* RSRP 256, EARFCN over the range, serving 2 */
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(
			      "0,\"074FEB50\",\"23002\",\"05F2\",31,941054,6300,488,256,17,1,1,0",
			      &param),
		      -ERANGE, "RSRP 256");
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(
			      "0,\"074FEB50\",\"23002\",\"05F2\",31,941054,262144,488,67,17,1,1,0",
			      &param),
		      -ERANGE, "EARFCN range");
	zassert_equal(
		ctr_lte_v2_parse_urc_ncellmeas(
			"0,\"074FEB50\",\"23002\",\"05F2\",31,941054,6300,488,67,17,1,2,0", &param),
		-ERANGE, "serving 2");
	/* A neighbour with the RSRQ out of the range */
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(CELL0("1") ",6300,339,64,256,0", &param),
		      -ERANGE, "neighbour RSRQ 256");
	expect_empty();
}

ZTEST(parser, test_ncellmeas_malformed)
{
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(NULL, &param), -EINVAL, "NULL line");
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_REGISTERED, NULL), -EINVAL,
		      "NULL param");
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas("", &param), -EBADMSG, "empty");
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas("x", &param), -EBADMSG, "no status");
	/* A trailing separator announces a cell that is not there */
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_NBIOT_CAMPED ",", &param),
		      -EBADMSG, "trailing separator");
	/* Fewer neighbours than announced */
	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(CELL0("2") SYN_NCELL, &param), -EPROTO,
		      "missing neighbour");
}

/* A failure leaves no half-parsed cells; the FSM owns result and uptime_s */
ZTEST(parser, test_ncellmeas_error_clears_the_cells)
{
	memset(&param, 0, sizeof(param));
	param.result = -ETIMEDOUT;
	param.uptime_s = 77;

	zassert_equal(ctr_lte_v2_parse_urc_ncellmeas(SYN_NCELLMEAS_CUT, &param), -EPROTO,
		      "cut after the PLMN");
	expect_empty();
	zassert_equal(param.result, -ETIMEDOUT, "result untouched");
	zassert_equal(param.uptime_s, 77, "uptime_s untouched");

	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_NBIOT_CAMPED, &param));
	zassert_equal(param.result, -ETIMEDOUT, "result untouched on success");
	zassert_equal(param.uptime_s, 77, "uptime_s untouched on success");
}

/* A second parse into the same structure does not keep the first one */
ZTEST(parser, test_ncellmeas_reparse)
{
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_REGISTERED, &param));
	zassert_ok(ctr_lte_v2_parse_urc_ncellmeas(HW_NCELLMEAS_SEARCHING, &param));
	zassert_equal(param.num_cells, 1, "num_cells");
	zassert_equal(param.num_ncells, 1, "num_ncells");
	zassert_equal(param.cells[1].eci, 0, "old cell dropped");
}

/* -------- +COPS=? -------- */

static struct ctr_lte_v2_scan_network nets[CTR_LTE_V2_SCAN_NETWORK_MAX];
static size_t net_count;

static void expect_net(int i, const char *plmn, uint8_t stat, uint8_t act)
{
	zassert_str_equal(nets[i].plmn, plmn, "nets[%d].plmn", i);
	zassert_equal(nets[i].stat, stat, "nets[%d].stat", i);
	zassert_equal(nets[i].act, act, "nets[%d].act", i);
}

static int cops(const char *s)
{
	memset(nets, 0, sizeof(nets));
	net_count = 99;

	return ctr_lte_v2_parse_cops_list(s, nets, ARRAY_SIZE(nets), &net_count);
}

ZTEST(parser, test_cops_hw_lte_m)
{
	zassert_ok(cops(HW_COPS_LIST));

	zassert_equal(net_count, 3, "count");
	expect_net(0, "23001", 1, 7);
	expect_net(1, "23003", 1, 7);
	expect_net(2, "23002", 2, 7);
}

ZTEST(parser, test_cops_hw_nbiot_camped)
{
	zassert_ok(cops(HW_COPS_NBIOT_CAMPED));

	zassert_equal(net_count, 3, "count");
	expect_net(0, "23003", 1, 9);
	expect_net(1, "23001", 2, 9);
	expect_net(2, "23002", 1, 9);
}

ZTEST(parser, test_cops_hw_psm_nbiot)
{
	zassert_ok(cops(HW_COPS_PSM_NBIOT));

	zassert_equal(net_count, 3, "count");
	expect_net(0, "23003", 2, 9);
	expect_net(1, "23002", 1, 9);
	expect_net(2, "23001", 1, 9);
}

ZTEST(parser, test_cops_names_and_3digit_mnc)
{
	zassert_ok(cops("(3,\"CHN-UNICOM\",\"UNICOM\",\"460001\",9)"));

	zassert_equal(net_count, 1, "count");
	expect_net(0, "460001", 3, 9);
}

/* 3GPP allows the lists of the supported modes and formats after ",," */
ZTEST(parser, test_cops_trailing_lists)
{
	zassert_ok(cops("(1,\"\",\"\",\"23003\",7),,(0,1,2,3,4),(0,1,2)"));
	zassert_equal(net_count, 1, "count");
	expect_net(0, "23003", 1, 7);

	zassert_ok(
		cops("(1,\"\",\"\",\"23003\",7),(2,\"\",\"\",\"23001\",7),,(0,1,2,3,4),(0,1,2)"));
	zassert_equal(net_count, 2, "count with two networks");
	expect_net(1, "23001", 2, 7);
}

ZTEST(parser, test_cops_empty)
{
	zassert_ok(cops(""));
	zassert_equal(net_count, 0, "count");
}

/* More networks than we keep: all are counted, the first ones are stored, the line is not copied */
ZTEST(parser, test_cops_more_than_max)
{
	zassert_ok(cops(SYN_COPS_LONG));

	zassert_true(strlen(SYN_COPS_LONG) > 384, "the line is long");
	zassert_equal(net_count, 12, "count");
	expect_net(0, "26201", 1, 7);
	expect_net(9, "26210", 1, 7);
}

ZTEST(parser, test_cops_max_smaller_than_list)
{
	struct ctr_lte_v2_scan_network one[1] = {0};
	size_t count = 0;

	zassert_ok(ctr_lte_v2_parse_cops_list(HW_COPS_LIST, one, 1, &count));
	zassert_equal(count, 3, "count");
	zassert_str_equal(one[0].plmn, "23001", "stored");
}

ZTEST(parser, test_cops_malformed)
{
	/* unquoted PLMN */
	zassert_equal(cops("(2,\"\",\"\",23002,7)"), -EBADMSG, "unquoted PLMN");
	/* PLMN of 7 characters, of 4 digits, with a letter */
	zassert_equal(cops("(2,\"\",\"\",\"2300123\",7)"), -EBADMSG, "PLMN too long");
	zassert_equal(cops("(2,\"\",\"\",\"2300\",7)"), -EBADMSG, "PLMN too short");
	zassert_equal(cops("(2,\"\",\"\",\"2300a\",7)"), -EBADMSG, "PLMN with a letter");
	/* missing closing parenthesis, status, AcT, names */
	zassert_equal(cops(SYN_COPS_BAD), -EBADMSG, "no closing parenthesis");
	zassert_equal(cops("(,\"\",\"\",\"23002\",7)"), -EBADMSG, "no status");
	zassert_equal(cops("(2,\"\",\"\",\"23002\",)"), -EBADMSG, "no AcT");
	zassert_equal(cops("(2,\"\",\"\")"), -EBADMSG, "no PLMN");
	zassert_equal(cops("(2,\"\",\"\",\"23002\",300)"), -EBADMSG, "AcT out of range");
	/* nothing is reported from a list that fails halfway */
	zassert_equal(cops("(1,\"\",\"\",\"23001\",7),(2,\"\",\"\",23002,7)"), -EBADMSG, "second");
	zassert_equal(net_count, 0, "count after an error");
	/* not a list */
	zassert_equal(cops("garbage"), -EBADMSG, "not a list");
	zassert_equal(ctr_lte_v2_parse_cops_list(NULL, nets, 1, &net_count), -EINVAL, "NULL line");
	zassert_equal(ctr_lte_v2_parse_cops_list("", NULL, 1, &net_count), -EINVAL, "NULL array");
	zassert_equal(ctr_lte_v2_parse_cops_list("", nets, 1, NULL), -EINVAL, "NULL count");
}
