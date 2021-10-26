#include "test_chester_x3.h"
#include <hio_chester_x3.h>

// Zephyr includes
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(test_chester_x3, LOG_LEVEL_DBG);

int test_chester_x3(void)
{
	int ret;

	ret = hio_chester_x3_init(HIO_CHESTER_X3_SLOT_A, HIO_CHESTER_X3_CHANNEL_1);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_x3_init` failed: %d", ret);
		return ret;
	}

	ret = hio_chester_x3_init(HIO_CHESTER_X3_SLOT_A, HIO_CHESTER_X3_CHANNEL_2);

	if (ret < 0) {
		LOG_ERR("Call `hio_chester_x3_init` failed: %d", ret);
		return ret;
	}

	for (;;) {
		LOG_INF("Loop");

		int32_t result;

		ret = hio_chester_x3_measure(HIO_CHESTER_X3_SLOT_A, HIO_CHESTER_X3_CHANNEL_1,
		                             &result);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_x3_measure` failed: %d", ret);
			return ret;
		}

		ret = hio_chester_x3_measure(HIO_CHESTER_X3_SLOT_A, HIO_CHESTER_X3_CHANNEL_2,
		                             &result);

		if (ret < 0) {
			LOG_ERR("Call `hio_chester_x3_measure` failed: %d", ret);
			return ret;
		}

		k_sleep(K_MSEC(1000));
	}

	return 0;
}
