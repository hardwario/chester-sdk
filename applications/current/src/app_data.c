#include "app_data.h"
#include "msg_key.h"

/* CHESTER includes */
#include <ctr_lte.h>

/* Standard includes */
#include <limits.h>
#include <math.h>

struct app_data g_app_data = {
	.batt_voltage_rest = NAN,
	.batt_voltage_load = NAN,
	.batt_current_load = NAN,
	.therm_temperature = NAN,
	.acceleration_x = NAN,
	.acceleration_y = NAN,
	.acceleration_z = NAN,
	.orientation = INT_MAX,
};

K_MUTEX_DEFINE(g_app_data_lte_eval_mut);
bool g_app_data_lte_eval_valid;
struct ctr_lte_eval g_app_data_lte_eval;
