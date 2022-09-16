#include <zephyr/logging/log.h>
#include <zephyr/zephyr.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

void main(void)
{
	LOG_INF("Build time: " __DATE__ " " __TIME__);
}
