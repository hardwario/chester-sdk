#ifndef CHESTER_SUBSYS_CTR_SAT_ASTRONODE_S_MESSAGES_H_
#define CHESTER_SUBSYS_CTR_SAT_ASTRONODE_S_MESSAGES_H_

/* Standard includes */
#include <stdint.h>

/* Zephyr includes */
#include <zephyr/sys/util.h>

#define ASTRONODE_S_CMD_CFG_W 0x05 /* Write configuration, and store in RAM */
#define ASTRONODE_S_CMD_WIF_W 0x06 /* Write Wi-Fi settings, and store in RAM - Wi-Fi DK only */
#define ASTRONODE_S_CMD_SSC_W                                                                      \
	0x07 /* Satellite Search Configuration Write Request. Stored in RAM (never saved in NVM).  \
	      */
#define ASTRONODE_S_CMD_CFG_S 0x10 /* Save configuration in NVM request */
#define ASTRONODE_S_CMD_CFG_F 0x11 /* Factory reset configuration request */
#define ASTRONODE_S_CMD_CFG_R 0x15 /* Read configuration from RAM */
#define ASTRONODE_S_CMD_RTC_R 0x17 /* Real Time Clock read request */
#define ASTRONODE_S_CMD_NCO_R 0x18 /* Next Contact Opportunity read request */
#define ASTRONODE_S_CMD_MGI_R 0x19 /* Module GUID read request */
#define ASTRONODE_S_CMD_MSN_R 0x1A /* Module Serial Number read request */
#define ASTRONODE_S_CMD_MPN_R 0x1B /* Module Product Number read request */
#define ASTRONODE_S_CMD_PLD_E                                                                      \
	0x25 /* Enqueue uplink payload, and store in RAM (never saved in NVM) */
#define ASTRONODE_S_CMD_PLD_D 0x26 /* Dequeue uplink payload */
#define ASTRONODE_S_CMD_PLD_F 0x27 /* Clear (Free) all queued payloads */
#define ASTRONODE_S_CMD_GEO_W                                                                      \
	0x35			   /* Write geolocation longitude and latitude, and store in RAM.  \
				    */
#define ASTRONODE_S_CMD_SAK_R 0x45 /* Read Acknowledgement */
#define ASTRONODE_S_CMD_SAK_C                                                                      \
	0x46 /* Confirm to the module that the Acknowledgement was properly decoded and can be     \
		deleted by the module */
#define ASTRONODE_S_CMD_CMD_R 0x47 /* Read a command (downlink) */
#define ASTRONODE_S_CMD_CMD_C                                                                      \
	0x48 /* Confirm to the module that the command was properly decoded and can be deleted by  \
		the module */
#define ASTRONODE_S_CMD_RES_C 0x55 /* Clear reset event */
#define ASTRONODE_S_CMD_VAL_W 0x60 /* Validation Mode Write */
#define ASTRONODE_S_CMD_TTX_S 0x61 /* Test Transmit Start Request */
#define ASTRONODE_S_CMD_GPO_S 0x62 /* GPIO Set Request */
#define ASTRONODE_S_CMD_GPI_R 0x63 /* GPIO Read Request */
#define ASTRONODE_S_CMD_ADC_R 0x64 /* ADC Read Request */
#define ASTRONODE_S_CMD_EVT_R 0x65 /* Reads the event register */
#define ASTRONODE_S_CMD_CTX_S 0x66 /* Context Save Request - recommended before cutting power */
#define ASTRONODE_S_CMD_PER_R 0x67 /* Performance Counter Read Request */
#define ASTRONODE_S_CMD_PER_C 0x68 /* Performance Counter Clear Request */
#define ASTRONODE_S_CMD_MST_R 0x69 /* Module State Read Request */
#define ASTRONODE_S_CMD_LCD_R 0x6A /* Last Contact Details Read Request */
#define ASTRONODE_S_CMD_END_R                                                                      \
	0x6B /* Environment Details Read Request to evaluate RF environment                        \
	      */
#define ASTRONODE_S_CMD_HTX_S                                                                      \
	0x6C /* Homologation Transmit Start Request (special firmware only)                        \
	      */

#define ASTRONODE_S_ANSWER_CODE(command_id) ((command_id) | 0x80)
#define ASTRONODE_S_ANSWER_ERROR	    0xFF

#define ASTRONODE_S_ERROR_CRC_NOT_VALID                                                            \
	0x0001 /* Discrepancy between provided CRC and expected CRC.*/
#define ASTRONODE_S_ERROR_LENGTH_NOT_VALID                                                         \
	0x0011 /* Message length is not valid for the operation code, or the message exceeds the   \
		  maximum length for a frame.*/
#define ASTRONODE_S_ERROR_OPCODE_NOT_VALID     0x0121 /* Invalid Operation Code used.*/
#define ASTRONODE_S_ERROR_ARG_NOT_VALID	       0x0122 /* Invalid argument used.*/
#define ASTRONODE_S_ERROR_FLASH_WRITING_FAILED 0x0123 /* Failed to write to the flash.*/
#define ASTRONODE_S_ERROR_DEVICE_BUSY	       0x0124 /* Device is busy.*/
#define ASTRONODE_S_ERROR_FORMAT_NOT_VALID                                                         \
	0x0601 /* At least one of the fields (SSID, password, token) is not composed of            \
		  exclusively printable standard ASCII characters (0x20 to 0x7E).*/
#define ASTRONODE_S_ERROR_PERIOD_INVALID                                                           \
	0x0701 /* The Satellite Search Config period enumeration value is not valid*/
#define ASTRONODE_S_ERROR_BUFFER_FULL                                                              \
	0x2501 /* Failed to queue the payload because the sending queue is already full.*/
#define ASTRONODE_S_ERROR_DUPLICATE_ID                                                             \
	0x2511 /* Failed to queue the payload because the Payload ID provided by the asset is      \
		  already in use in the module queue.*/
#define ASTRONODE_S_ERROR_BUFFER_EMPTY                                                             \
	0x2601 /* Failed to dequeue a payload from the buffer because the buffer is empty*/
#define ASTRONODE_S_ERROR_INVALID_POS                                                              \
	0x3501 /* Failed to update the geolocation information. Latitude and longitude fields must \
		  in the range [-90,90] degrees and [-180,180] degrees, respectively.*/
#define ASTRONODE_S_ERROR_NO_ACK                                                                   \
	0x4501 /* No satellite acknowledgement available for any                                   \
		  payload.*/
#define ASTRONODE_S_ERROR_NO_ACK_CLEAR                                                             \
	0x4601				    /* No payload ack to clear, or it was already cleared.*/
#define ASTRONODE_S_ERROR_NO_COMMAND 0x4701 /* No command is available.*/
#define ASTRONODE_S_ERROR_NO_COMMAND_CLEAR                                                         \
	0x4801 /* No command to clear, or it was already cleared.*/
#define ASTRONODE_S_ERROR_MAX_TX_REACHED                                                           \
	0x6101 /* Failed to test Tx due to the maximum number of transmissions being reached.*/

#define ASTRONODE_S_START_BYTE 0x02
#define ASTRONODE_S_END_BYTE   0x03

#define ASTRONODE_MAX_MESSAGE_LEN 160

#define ASTRONODE_MAX_RESET_WAKEUP_TIME_MSEC 400

#define ASTRONODE_S_CFG1_ACK_EN_MASK		BIT(0)
#define ASTRONODE_S_CFG1_ADD_GEO_MASK		BIT(1)
#define ASTRONODE_S_CFG1_EPHEMERIS_EN_MASK	BIT(2)
#define ASTRONODE_S_CFG1_DEEPSLEEP_EN_MASK	BIT(3)
#define ASTRONODE_S_CFG3_INT_ACK_EN_MASK	BIT(0)
#define ASTRONODE_S_CFG3_INT_RST_EN_MASK	BIT(1)
#define ASTRONODE_S_CFG3_INT_CMD_EN_MASK	BIT(2)
#define ASTRONODE_S_CFG3_INT_TX_PENDING_EN_MASK BIT(3)

#define ASTRONODE_S_CFG_RA_PRODUCT_ID	   0
#define ASTRONODE_S_CFG_RA_HW_REV	   1
#define ASTRONODE_S_CFG_RA_FW_VER_MAJOR	   2
#define ASTRONODE_S_CFG_RA_FW_VER_MINOR	   3
#define ASTRONODE_S_CFG_RA_FW_VER_REVISION 4
#define ASTRONODE_S_CFG_RA_CFG1		   5
#define ASTRONODE_S_CFG_RA_CFG2		   6
#define ASTRONODE_S_CFG_RA_CFG3		   7

#define ASTRONODE_S_CFG_WR_CFG1 0
#define ASTRONODE_S_CFG_WR_CFG2 1
#define ASTRONODE_S_CFG_WR_CFG3 2

typedef struct {
	/* empty */
} astronode_cfg_r_request;

enum {
	ASTRONODE_S_EVENT_SAT_ACK = BIT(0),
	ASTRONODE_S_EVENT_MODULE_RESET = BIT(1),
	ASTRONODE_S_EVENT_COMMAND_AVAILABLE = BIT(2),
	ASTRONODE_S_EVENT_MESSAGE_TRANSMIT_PENDING = BIT(3)
} astronode_s_event;

typedef struct {
	uint8_t ssid[33];
	uint8_t password[64];
	uint8_t api_key[97];
} __attribute__((packed)) astronode_wfi_w_request;

typedef struct {
	uint16_t id;
	uint8_t message[160];
} __attribute__((packed)) astronode_pld_e_request;

typedef struct {
	uint16_t id;
} __attribute__((packed)) astronode_pld_e_answer;

typedef struct {
	uint16_t error_code;
} __attribute__((packed)) astronode_error_answer;

#endif /* CHESTER_SUBSYS_CTR_SAT_ASTRONODE_S_MESSAGES_H_ */