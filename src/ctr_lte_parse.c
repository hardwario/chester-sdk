#include <ctr_lte_parse.h>
#include <ctr_lte_tok.h>

/* Zephyr includes */
#include <zephyr.h>

/* Standard includes */
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

int ctr_lte_parse_cclk(const char *s, int *year, int *month, int *day, int *hours, int *minutes,
                       int *seconds)
{
	/* +CCLK: "21/11/08,16:01:13+04" */

	char *p = (char *)s;

	if ((p = ctr_lte_tok_pfx(p, "+CCLK: ")) == NULL) {
		return -EINVAL;
	}

	bool def;
	char buf[20 + 1];

	if ((p = ctr_lte_tok_str(p, &def, buf, sizeof(buf))) == NULL) {
		return -EINVAL;
	}

	if (!def) {
		return -EINVAL;
	}

	if (strlen(buf) != 20) {
		return -EINVAL;
	}

	if (!isdigit((int)buf[0]) || !isdigit((int)buf[1]) || buf[2] != '/' ||
	    !isdigit((int)buf[3]) || !isdigit((int)buf[4]) || buf[5] != '/' ||
	    !isdigit((int)buf[6]) || !isdigit((int)buf[7]) || buf[8] != ',' ||
	    !isdigit((int)buf[9]) || !isdigit((int)buf[10]) || buf[11] != ':' ||
	    !isdigit((int)buf[12]) || !isdigit((int)buf[13]) || buf[14] != ':' ||
	    !isdigit((int)buf[15]) || !isdigit((int)buf[16]) ||
	    (buf[17] != '+' && buf[17] != '-') || !isdigit((int)buf[18]) ||
	    !isdigit((int)buf[19])) {
		return -EINVAL;
	}

	buf[2] = buf[5] = buf[8] = buf[11] = buf[14] = buf[17] = '\0';

	if (year != NULL) {
		*year = atoi(&buf[0]);
	}

	if (month != NULL) {
		*month = atoi(&buf[3]);
	}

	if (day != NULL) {
		*day = atoi(&buf[6]);
	}

	if (hours != NULL) {
		*hours = atoi(&buf[9]);
	}

	if (minutes != NULL) {
		*minutes = atoi(&buf[12]);
	}

	if (seconds != NULL) {
		*seconds = atoi(&buf[15]);
	}

	return 0;
}
