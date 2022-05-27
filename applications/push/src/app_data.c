#include "app_data.h"

/* Standard includes */
#include <stdbool.h>

struct app_data g_app_data = {
	.errors = {
		.orientation = true,
		.int_temperature = true,
		.ext_temperature = true,
		.ext_humidity = true,
		.line_present = true,
		.line_voltage = true,
		.bckp_voltage = true,
		.batt_voltage_rest = true,
		.batt_voltage_load = true,
		.batt_current_load = true,
        },
};
