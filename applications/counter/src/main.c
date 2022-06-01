#include "app_init.h"
#include "app_loop.h"

/* Zephyr includes */
#include <logging/log.h>
#include <zephyr.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

void main(void)
{
	int ret;

	LOG_INF("Build time: " __DATE__ " " __TIME__);

	ret = app_init();
	if (ret) {
		LOG_ERR("Call `app_init` failed: %d", ret);
		k_oops();
	}

	for (;;) {
		LOG_INF("Alive");

		ret = app_loop();
		if (ret) {
			LOG_ERR("Call `app_loop` failed: %d", ret);
			k_oops();
		}
	}
}
