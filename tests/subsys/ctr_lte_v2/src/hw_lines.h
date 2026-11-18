/** @file
 *  @brief Modem lines captured on hardware (CHESTER-U1, mfw 1.3.6, SLM 2.5.2), shared by tests
 *
 *  HW_* lines are verbatim captures, SYN_* lines are synthetic (the comment says why).
 */

#ifndef CHESTER_TESTS_CTR_LTE_V2_HW_LINES_H_
#define CHESTER_TESTS_CTR_LTE_V2_HW_LINES_H_

/* %NCELLMEAS=5,7 after the registration, in the RRC idle: 4 cells in 3 PLMNs */
#define HW_NCELLMEAS_REGISTERED                                                                    \
	"0,\"074FEB50\",\"23002\",\"05F2\",31,941054,6300,488,67,17,1139290,1,0,"                  \
	"\"074FEB51\",\"23002\",\"05F2\",65535,0,6300,339,65,12,1139290,0,0,"                      \
	"\"074FEB0C\",\"23001\",\"3866\",65535,0,6200,14,66,14,1139326,0,0,"                       \
	"\"000AE5CA\",\"23003\",\"8DCC\",65535,0,3544,135,75,32,1139335,0,0"

/* %NCELLMEAS=5,7 while searching the network (+CEREG: 2): the serving cell and a neighbour */
#define HW_NCELLMEAS_SEARCHING                                                                     \
	"0,\"074FEB50\",\"23002\",\"05F2\",17,80345,6300,488,68,19,73670,1,1,6300,339,64,12,0"

/* %NCELLMEAS=5,7 with the RRC connection still open (before +CSCON: 0) */
#define HW_NCELLMEAS_CONNECTED                                                                     \
	"0,\"074FEB50\",\"23002\",\"05F2\",15,12872307,6300,488,67,15,12865064,1,1,6300,339,66,"   \
	"14,0"

/* The URC sent at once by %NCELLMEASSTOP during a measurement: the 3 cells found so far */
#define HW_NCELLMEAS_STOPPED                                                                       \
	"0,\"074FEB50\",\"23002\",\"05F2\",15,12887435,6300,488,67,21,12913708,1,0,"               \
	"\"074FEB51\",\"23002\",\"05F2\",65535,0,6300,339,66,19,12913708,0,0,"                     \
	"\"000AE5CA\",\"23003\",\"8DCC\",65535,0,3544,135,75,29,12913846,0,0"

/* %NCELLMEAS=5,7 camped on NB-IoT without a registration: one cell */
#define HW_NCELLMEAS_NBIOT_CAMPED                                                                  \
	"0,\"074FEBAC\",\"23001\",\"28B4\",0,13115210,6247,14,73,21,13116648,1,0"

/* %NCELLMEAS=5,7 in PSM (the board with the SIM of 23003, gate 3A, A1): after 118 s, three
 * cells and none of them serving */
#define HW_PSM_NCELLMEAS                                                                           \
	"0,\"000AE520\",\"23003\",\"B4DC\",65535,0,6447,135,88,28,647446,0,0,"                     \
	"\"074FEB56\",\"23002\",\"2D02\",65535,0,6348,339,72,26,647523,0,0,"                       \
	"\"074FEBAC\",\"23001\",\"28B4\",65535,0,6247,14,77,28,647603,0,0"

/* The URC sent at once by %NCELLMEASSTOP in PSM (gate 3A, A2): the one cell found so far */
#define HW_PSM_NCELLMEAS_STOPPED                                                                   \
	"0,\"000AE520\",\"23003\",\"B4DC\",65535,0,6447,135,88,29,658953,0,0"

/* A neighbour (earfcn, pci, rsrp, rsrq, time_diff) to build lines with 10 and more of them */
#define SYN_NCELL ",6300,339,64,12,0"

/* The first cell of HW_NCELLMEAS_REGISTERED followed by a second one cut after the PLMN */
#define SYN_NCELLMEAS_CUT                                                                          \
	"0,\"074FEB50\",\"23002\",\"05F2\",31,941054,6300,488,67,17,1139290,1,0,"                  \
	"\"074FEB51\",\"23002\""

#endif /* CHESTER_TESTS_CTR_LTE_V2_HW_LINES_H_ */
