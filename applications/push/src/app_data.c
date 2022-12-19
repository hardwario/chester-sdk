#include "app_data.h"

/* CHESTER includes */
#if defined(CONFIG_SHIELD_CTR_LTE)
#include <chester/ctr_lte.h>
#endif

/* Standard includes */
#include <stdbool.h>

struct app_data g_app_data = {
	.errors =
		{
			.orientation = true,
			.acceleration_x = true,
			.acceleration_y = true,
			.acceleration_z = true,
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

#if defined(CONFIG_SHIELD_CTR_LTE)
K_MUTEX_DEFINE(g_app_data_lte_eval_mut);
bool g_app_data_lte_eval_valid;
struct ctr_lte_eval g_app_data_lte_eval;
#endif
