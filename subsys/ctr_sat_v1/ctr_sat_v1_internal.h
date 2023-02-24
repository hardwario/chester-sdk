#ifndef CHESTER_SUBSYS_CTR_SAT_V1_CTR_SAT_V1_INTERNAL_H_
#define CHESTER_SUBSYS_CTR_SAT_V1_CTR_SAT_V1_INTERNAL_H_

/* CHESTER includes */
#include <chester/ctr_sat_v1.h>

/*
	flags definition as used in ctr_sat_execute_command function. Flags are used only for
   internal pusrpose of this subsys module and are not intended for use by user.

	bit 0: FLAG_NO_WARN_ASTRONODE_ERROR - used when error reply message is not directly error.
   Cleans log from misleading error messages. When this bit is set, ensure to set
   FLAG_ALLOW_UNEXPECTED_LENGTH also becase error messages usualy have different length than
   standard reply.

	bit 1: FLAG_REPEAT_ON_REQ_CRC_ERROR: - allows retrying exectuion of command after receiving
   valid reply containing CRC error message (meaning that CRC error happended when transfering
   request).

	bit 2: FLAG_REPEAT_ON_ANS_CRC_ERROR: - allows retrying exectuion of command exectuion when
   astronode respond with any message and local CRC check fails.

	bit 7:3: FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES: - number of allowed repetition of command
   exectuion when transmission of request or answer is affected by CRC error. For enabling this
   feature you need to specify REPEAT_ON_REQ_CRC_ERROR or REPEAT_ON_ANS_CRC_ERROR or both.

	bit 8: FLAG_ALLOW_UNEXPECTED_LENGTH - used for preventing log warning when received message
   has unexpected length. Cleans log for misleading messages.

	bit 9: FLAG_TRIM_LONG_REPLY - used for commands with TLV structure which can be extended in
   future. This flag allow trimming (future) messages.

   Do not use FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT, FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT and
   FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK macros directly. Instead use
   FLAG_REPEAT_ON_REQ_CRC_ERRORS, FLAG_REPEAT_ON_ANS_CRC_ERRORS or FLAG_REPEAT_ON_ALL_CRC_ERRORS
   which contains embeded both flag and retry count defined by standard Kconfig option.
*/
#define FLAG_NO_WARN_ASTRONODE_ERROR		  BIT(0)
#define FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT	  BIT(1)
#define FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT	  BIT(2)
#define FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK GENMASK(7, 3)
#define FLAG_ALLOW_UNEXPECTED_LENGTH		  BIT(8)
#define FLAG_TRIM_LONG_REPLY			  BIT(9)

#define FLAG_REPEAT_ON_REQ_CRC_ERRORS                                                              \
	(FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT |                                                        \
	 FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK,                                     \
		    CONFIG_CTR_SAT_V1_CRC_ERRORS_ALLOWED_RETRIES))

#define FLAG_REPEAT_ON_ANS_CRC_ERRORS                                                              \
	(FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT |                                                        \
	 FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK,                                     \
		    CONFIG_CTR_SAT_V1_CRC_ERRORS_ALLOWED_RETRIES))

#define FLAG_REPEAT_ON_ALL_CRC_ERRORS                                                              \
	(FLAG_REPEAT_ON_REQ_CRC_ERROR_BIT | FLAG_REPEAT_ON_ANS_CRC_ERROR_BIT |                     \
	 FIELD_PREP(FLAG_REPEAT_ON_CRC_ERROR_MAX_RETRIES_MASK,                                     \
		    CONFIG_CTR_SAT_V1_CRC_ERRORS_ALLOWED_RETRIES))

/* Invalid CRC error code used for distinguishing from other EIO errors.*/
#define E_INVCRC 10000

int ctr_sat_v1_init_generic(struct ctr_sat *sat);
int ctr_sat_v1_execute_command(struct ctr_sat *sat, uint8_t command, void *parameters,
			       size_t parameters_size, void *response_data,
			       size_t *response_data_size, int flags);

#endif /* CHESTER_SUBSYS_CTR_SAT_V1_CTR_SAT_V1_INTERNAL_H_ */
